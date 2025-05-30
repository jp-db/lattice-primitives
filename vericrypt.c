#include "vericrypt.h"
#include "param.h"
#include "commit.h"
#include "test.h"
#include "bench.h"
#include "gaussian.h"
#include "fastrandombytes.h"
#include "sha.h"

static int vericrypt_test_norm(veritext_t *out, encryption_scheme_t encryption_scheme) {
	int result;
	fmpz_t coeff, max, qdiv2, *q;
	fmpz_mod_poly_t t;
	fmpz_mod_ctx_t *ctx;

	fmpz_init(coeff);
	fmpz_init(max);
	fmpz_init(qdiv2);

	q = encrypt_large_modulus(encryption_scheme);
	ctx = encrypt_large_modulus_ctx(encryption_scheme);

	fmpz_mod_poly_init(t, *ctx);
	fmpz_set(qdiv2, *q);
	fmpz_divexact_ui(qdiv2, qdiv2, 2);
	fmpz_set_ui(max, 0);

	/* Compute norm_infty. */
	for (int i = 0; i < VECTOR; i++) {
		for (int j = 0; j < DIM; j++) {
			qcrt_poly_rec(encryption_scheme, t, out->r[i][j]);
			for (int k = 0; k < DEGREE; k++) {
				fmpz_mod_poly_get_coeff_fmpz(coeff, t, k, *ctx);
				if (fmpz_cmp(coeff, qdiv2) > 0)
					fmpz_sub(coeff, coeff, *q);
				fmpz_abs(coeff, coeff);
				if (fmpz_cmp(coeff, max) > 0)
					fmpz_set(max, coeff);
			}
			qcrt_poly_rec(encryption_scheme, t, out->e[i][j]);
			for (int k = 0; k < DEGREE; k++) {
				fmpz_mod_poly_get_coeff_fmpz(coeff, t, k, *ctx);
				if (fmpz_cmp(coeff, qdiv2) > 0)
					fmpz_sub(coeff, coeff, *q);
				fmpz_abs(coeff, coeff);
				if (fmpz_cmp(coeff, max) > 0)
					fmpz_set(max, coeff);
			}
		}
		qcrt_poly_rec(encryption_scheme, t, out->e_[i]);
		for (int k = 0; k < DEGREE; k++) {
			fmpz_mod_poly_get_coeff_fmpz(coeff, t, k, *ctx);
			if (fmpz_cmp(coeff, qdiv2) > 0)
				fmpz_sub(coeff, coeff, *q);
			fmpz_abs(coeff, coeff);
			if (fmpz_cmp(coeff, max) > 0)
				fmpz_set(coeff, max);
		}
	}
	for (int i = 0; i < VECTOR; i++) {
		for (int k = 0; k < DEGREE; k++) {
			fmpz_mod_poly_get_coeff_fmpz(coeff, out->u[i], k, *ctx);
			fmpz_abs(coeff, coeff);
			if (fmpz_cmp(coeff, max) > 0)
				fmpz_set(coeff, max);
		}
	}

	/* Actually compute sigma^2 to simplify comparison. */
	fmpz_set_ui(coeff, 6 * SIGMA_E);
	result = fmpz_cmp(max, coeff);

	fmpz_clear(coeff);
	fmpz_clear(max);
	fmpz_clear(qdiv2);
	return (result < 0);
}

/*============================================================================*/
/* Public definitions                                                         */
/*============================================================================*/

void vericrypt_free(veritext_t *out, encryption_scheme_t encryption_scheme) {
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_clear(out->u[i], *encrypt_modulus_ctx(encryption_scheme));
		for (int j = 0; j < DIM; j++) {
			fmpz_mod_poly_clear(out->e_[i][j], *encrypt_large_modulus_ctx(encryption_scheme));
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_clear(out->r[i][j][k],
						*encrypt_large_modulus_ctx(encryption_scheme));
				fmpz_mod_poly_clear(out->e[i][j][k],
						*encrypt_large_modulus_ctx(encryption_scheme));
			}
		}
	}
}

void vericrypt_hash(uint8_t hash[SHA256HashSize], encryption_scheme_t encryption_scheme, publickey_t *pk,
		fmpz_mod_poly_t t[VECTOR], fmpz_mod_poly_t u, veritext_t *out,
		ciphertext_t y[VECTOR], fmpz_mod_poly_t _u) {
	fmpz_poly_t s;
	fmpz_mod_ctx_t *ctx = encrypt_large_modulus_ctx(encryption_scheme);
	SHA256Context sha;
	char *str = NULL;

	fmpz_poly_init(s);
	SHA256Reset(&sha);

	/* Hash public key (A,t). */
	for (int i = 0; i < DIM; i++) {
		for (int j = 0; j < DIM; j++) {
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_get_fmpz_poly(s, pk->A[i][j][k], *ctx);
				str = fmpz_poly_get_str(s);
				SHA256Input(&sha, (const uint8_t *)str, strlen(str));
				free(str);
			}
			fmpz_mod_poly_get_fmpz_poly(s, pk->t[i][j], *ctx);
			str = fmpz_poly_get_str(s);
			SHA256Input(&sha, (const uint8_t *)str, strlen(str));
			free(str);
		}
	}
	/* Hash linear relation pair (T,u). */
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_get_fmpz_poly(s, t[i], *ctx);
		str = fmpz_poly_get_str(s);
		SHA256Input(&sha, (const uint8_t *)str, strlen(str));
		free(str);
	}
	fmpz_mod_poly_get_fmpz_poly(s, u, *ctx);
	str = fmpz_poly_get_str(s);
	SHA256Input(&sha, (const uint8_t *)str, strlen(str));
	free(str);

	/* Hash ciphertexts c = (v, w), y. */
	for (int i = 0; i < VECTOR; i++) {
		for (int j = 0; j < DIM; j++) {
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_get_fmpz_poly(s, out->cipher[i].v[j][k], *ctx);
				str = fmpz_poly_get_str(s);
				SHA256Input(&sha, (const uint8_t *)str, strlen(str));
				free(str);
				fmpz_mod_poly_get_fmpz_poly(s, y[i].v[j][k], *ctx);
				str = fmpz_poly_get_str(s);
				SHA256Input(&sha, (const uint8_t *)str, strlen(str));
				free(str);
			}
		}
		for (int k = 0; k < 2; k++) {
			fmpz_mod_poly_get_fmpz_poly(s, out->cipher[i].w[k], *ctx);
			str = fmpz_poly_get_str(s);
			SHA256Input(&sha, (const uint8_t *)str, strlen(str));
			free(str);
			fmpz_mod_poly_get_fmpz_poly(s, y[i].w[k], *ctx);
			str = fmpz_poly_get_str(s);
			SHA256Input(&sha, (const uint8_t *)str, strlen(str));
			free(str);
		}
	}
	fmpz_mod_poly_get_fmpz_poly(s, _u, *encrypt_modulus_ctx(encryption_scheme));
	str = fmpz_poly_get_str(s);
	SHA256Input(&sha, (const uint8_t *)str, strlen(str));
	free(str);

	SHA256Result(&sha, hash);
	fmpz_poly_clear(s);
}

void vericrypt_sample_chall(fmpz_mod_poly_t f, uint8_t *seed, int len,
		fmpz_mod_ctx_t *ctx) {
	fmpz_t coeff;
	uint32_t buf;

	fmpz_init(coeff);
	fmpz_mod_poly_zero(f, *ctx);
	fastrandombytes_setseed(seed);
	for (int j = 0; j < NONZERO; j++) {
		fastrandombytes((unsigned char *)&buf, sizeof(buf));
		buf = buf % DEGREE;
		fmpz_mod_poly_get_coeff_fmpz(coeff, f, buf, *ctx);
		while (!fmpz_is_zero(coeff)) {
			fastrandombytes((unsigned char *)&buf, sizeof(buf));
			buf = buf % DEGREE;
			fmpz_mod_poly_get_coeff_fmpz(coeff, f, buf, *ctx);
		}
		fmpz_set_ui(coeff, 1);
		fmpz_mod_poly_set_coeff_fmpz(f, buf, coeff, *ctx);
	}
	fmpz_clear(coeff);
}

void vericrypt_sample_gauss(fmpz_mod_poly_t r, fmpz_mod_ctx_t *ctx) {
	fmpz_t coeff, t;
	fmpz_init(coeff);
	fmpz_init(t);
	fmpz_mod_poly_zero(r, *ctx);
	fmpz_mod_poly_fit_length(r, DEGREE, *ctx);
    int64_t samples[DEGREE];
    discrete_gaussian_vec(samples, 0.0, DEGREE);
	for (int i = 0; i < DEGREE; i++) {
		fmpz_set_si(t, samples[i]);
		fmpz_mod_poly_set_coeff_fmpz(r, i, t, *ctx);
	}
	fmpz_clear(coeff);
	fmpz_clear(t);
}

void vericrypt_sample_gauss_crt(fmpz_mod_poly_t r[2], encryption_scheme_t encryption_scheme, fmpz_mod_ctx_t *ctx) {
	fmpz_mod_poly_t t;

	fmpz_mod_poly_init(t, *ctx);
	vericrypt_sample_gauss(t, ctx);
	fmpz_mod_poly_rem(r[0], t, *encrypt_irred(encryption_scheme, 0), *ctx);
	fmpz_mod_poly_rem(r[1], t, *encrypt_irred(encryption_scheme, 1), *ctx);

	fmpz_mod_poly_clear(t, *encrypt_large_modulus_ctx(encryption_scheme));
}

int vericrypt_doit(veritext_t *out, fmpz_mod_poly_t t[VECTOR],
		fmpz_mod_poly_t u, fmpz_mod_poly_t m[VECTOR], encryption_scheme_t encryption_scheme,
        publickey_t *pk, flint_rand_t rand) {
	fmpz_mod_poly_t _u, tmp, c[DIM], y_mu[VECTOR];
	qcrt_poly_t y_r[VECTOR][DIM], y_e[VECTOR][DIM], y_e_[VECTOR];
	ciphertext_t y[VECTOR];
	uint8_t hash[SHA256HashSize];
	int result = 0;

	fmpz_mod_ctx_t *ctx_p = encrypt_modulus_ctx(encryption_scheme);
	fmpz_mod_ctx_t *ctx_q = encrypt_large_modulus_ctx(encryption_scheme);

	fmpz_mod_poly_init(tmp, *ctx_p);
	fmpz_mod_poly_init(_u, *ctx_p);
	fmpz_mod_poly_init(out->c, *ctx_p);
	fmpz_mod_poly_init(c[0], *ctx_p);
	fmpz_mod_poly_init(c[1], *ctx_p);
	for (int i = 0; i < VECTOR; i++) {
		for (int j = 0; j < DIM; j++) {
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_init(y_r[i][j][k], *ctx_q);
				fmpz_mod_poly_init(y_e[i][j][k], *ctx_q);
				fmpz_mod_poly_init(out->r[i][j][k], *ctx_q);
				fmpz_mod_poly_init(out->e[i][j][k], *ctx_q);
			}
			fmpz_mod_poly_init(out->e_[i][j], *ctx_q);
			fmpz_mod_poly_init(y_e_[i][j], *ctx_q);
		}
		fmpz_mod_poly_init(out->u[i], *ctx_p);
		fmpz_mod_poly_init(y_mu[i], *ctx_p);
	}

	for (int i = 0; i < VECTOR; i++) {
		for (int j = 0; j < DIM; j++) {
			encrypt_sample_short_crt(encryption_scheme, out->r[i][j], *ctx_q);
			encrypt_sample_short_crt(encryption_scheme, out->e[i][j], *ctx_q);
		}
		encrypt_sample_short_crt(encryption_scheme, out->e_[i], *ctx_q);
	}

	for (int i = 0; i < VECTOR; i++) {
		encrypt_make(encryption_scheme, &out->cipher[i], out->r[i], out->e[i], out->e_[i], m[i],
				pk);
	}

	while (result == 0) {
		for (int i = 0; i < VECTOR; i++) {
			for (int j = 0; j < DIM; j++) {
				vericrypt_sample_gauss_crt(y_r[i][j], encryption_scheme, ctx_q);
				vericrypt_sample_gauss_crt(y_e[i][j], encryption_scheme, ctx_q);
			}
			vericrypt_sample_gauss_crt(y_e_[i], encryption_scheme, ctx_q);
			vericrypt_sample_gauss(y_mu[i], ctx_p);
		}

		for (int i = 0; i < VECTOR; i++) {
			encrypt_make(encryption_scheme, &y[i], y_r[i], y_e[i], y_e_[i], y_mu[i], pk);
		}

		fmpz_mod_poly_init(_u, *ctx_p);
		for (int i = 0; i < VECTOR; i++) {
			fmpz_mod_poly_mulmod(tmp, t[i], y_mu[i], *encrypt_poly(encryption_scheme), *ctx_p);
			fmpz_mod_poly_add(_u, _u, tmp, *ctx_p);
		}

		/* Hash and convert result to challenge space. */
		vericrypt_hash(hash, encryption_scheme, pk, t, u, out, y, _u);
		vericrypt_sample_chall(out->c, hash, SHA256HashSize, ctx_p);
		fmpz_mod_poly_rem(c[0], out->c, *encrypt_irred(encryption_scheme, 0), *ctx_q);
		fmpz_mod_poly_rem(c[1], out->c, *encrypt_irred(encryption_scheme, 1), *ctx_q);

		// Compute z = [r e e' mu]^T c + y.
		for (int i = 0; i < VECTOR; i++) {
			for (int j = 0; j < DIM; j++) {
				for (int k = 0; k < 2; k++) {
					fmpz_mod_poly_mulmod(out->r[i][j][k], out->r[i][j][k], c[k],
							*encrypt_irred(encryption_scheme, k), *ctx_q);
					fmpz_mod_poly_add(out->r[i][j][k], out->r[i][j][k],
							y_r[i][j][k], *ctx_q);
					fmpz_mod_poly_mulmod(out->e[i][j][k], out->e[i][j][k], c[k],
							*encrypt_irred(encryption_scheme, k), *ctx_q);
					fmpz_mod_poly_add(out->e[i][j][k], out->e[i][j][k],
							y_e[i][j][k], *ctx_q);
				}
			}
		}
		for (int i = 0; i < VECTOR; i++) {
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_mulmod(out->e_[i][k], out->e_[i][k], c[k],
						*encrypt_irred(encryption_scheme, k), *ctx_q);
				fmpz_mod_poly_add(out->e_[i][k], out->e_[i][k], y_e_[i][k],
						*ctx_q);
			}
			fmpz_mod_poly_mulmod(out->u[i], m[i], out->c, *encrypt_poly(encryption_scheme),
					*ctx_p);
			fmpz_mod_poly_add(out->u[i], out->u[i], y_mu[i], *ctx_p);
		}

		result = vericrypt_test_norm(out, encryption_scheme);
	}

	fmpz_mod_poly_clear(tmp, *ctx_p);
	fmpz_mod_poly_clear(c[0], *ctx_p);
	fmpz_mod_poly_clear(c[1], *ctx_p);
	for (int i = 0; i < VECTOR; i++) {
		for (int j = 0; j < DIM; j++) {
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_clear(y_r[i][j][k], *ctx_q);
				fmpz_mod_poly_clear(y_e[i][j][k], *ctx_q);
			}
			fmpz_mod_poly_clear(y_e_[i][j], *ctx_q);
		}
		fmpz_mod_poly_clear(y_mu[i], *ctx_p);
	}
	return result;
}

int vericrypt_verify(veritext_t *in, fmpz_mod_poly_t t[VECTOR],
		fmpz_mod_poly_t u, encryption_scheme_t  encryption_scheme, publickey_t *pk) {
	fmpz_mod_poly_t _u, tp, tq, c;
	qcrt_poly_t _c;
	ciphertext_t y[VECTOR];
	int result = 0;
	uint8_t hash[SHA256HashSize];

	fmpz_mod_ctx_t *ctx_p = encrypt_modulus_ctx(encryption_scheme);
	fmpz_mod_ctx_t *ctx_q = encrypt_large_modulus_ctx(encryption_scheme);

	fmpz_mod_poly_init(c, *ctx_p);
	fmpz_mod_poly_init(tq, *ctx_q);
	fmpz_mod_poly_init(tp, *ctx_p);
	fmpz_mod_poly_init(_u, *ctx_p);
	for (int i = 0; i < DIM; i++) {
		fmpz_mod_poly_init(_c[i], *ctx_q);
	}

	if (vericrypt_test_norm(in, encryption_scheme)) {
		for (int i = 0; i < VECTOR; i++) {
			encrypt_make(encryption_scheme, &y[i], in->r[i], in->e[i], in->e_[i], in->u[i], pk);
		}
		fmpz_mod_poly_rem(_c[0], in->c, *encrypt_irred(encryption_scheme, 0), *ctx_q);
		fmpz_mod_poly_rem(_c[1], in->c, *encrypt_irred(encryption_scheme, 1), *ctx_q);

		fmpz_mod_poly_zero(_u, *ctx_p);
		for (int i = 0; i < VECTOR; i++) {
			fmpz_mod_poly_mulmod(tp, t[i], in->u[i], *encrypt_poly(encryption_scheme), *ctx_p);
			fmpz_mod_poly_add(_u, _u, tp, *ctx_p);
		}
		fmpz_mod_poly_mulmod(tp, in->c, u, *encrypt_poly(encryption_scheme), *ctx_p);
		fmpz_mod_poly_sub(_u, _u, tp, *ctx_p);

		for (int i = 0; i < VECTOR; i++) {
			for (int j = 0; j < DIM; j++) {
				for (int k = 0; k < 2; k++) {
					fmpz_mod_poly_mulmod(tq, _c[k], in->cipher[i].v[j][k],
							*encrypt_irred(encryption_scheme, k), *ctx_q);
					fmpz_mod_poly_sub(y[i].v[j][k], y[i].v[j][k], tq, *ctx_q);
				}
			}
			for (int k = 0; k < 2; k++) {
				fmpz_mod_poly_mulmod(tq, _c[k], in->cipher[i].w[k],
						*encrypt_irred(encryption_scheme, k), *ctx_q);
				fmpz_mod_poly_sub(y[i].w[k], y[i].w[k], tq, *ctx_q);
			}
		}

		vericrypt_hash(hash, encryption_scheme, pk, t, u, in, y, _u);
		vericrypt_sample_chall(c, hash, SHA256HashSize, ctx_p);

		result = fmpz_mod_poly_equal(in->c, c, *ctx_p);
	}

	fmpz_mod_poly_clear(c, *ctx_p);
	fmpz_mod_poly_clear(tp, *ctx_p);
	fmpz_mod_poly_clear(tq, *ctx_q);
	for (int i = 0; i < DIM; i++) {
		fmpz_mod_poly_clear(_c[i], *ctx_q);
	}
	return (result == 1);
}

int vericrypt_undo(fmpz_mod_poly_t m[VECTOR], fmpz_mod_poly_t c,
		veritext_t *in, fmpz_mod_poly_t t[VECTOR], fmpz_mod_poly_t u,
		encryption_scheme_t encryption_scheme, publickey_t *pk, privatekey_t *sk) {
	int result = 1;

	if (vericrypt_verify(in, t, u, encryption_scheme, pk) == 0) {
		return 0;
	}

	encrypt_sample_short(c, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_sub(c, in->c, c, *encrypt_modulus_ctx(encryption_scheme));

	for (int i = 0; i < VECTOR; i++) {
		result &= encrypt_undo(encryption_scheme, m[i], c, &in->cipher[i], sk);
	}

	return result;
}

void vericrypt_cipher_clear(veritext_t *in, encryption_scheme_t encryption_scheme) {
    for (int i = 0; i < VECTOR; i++) {
        encrypt_free(encryption_scheme, &in->cipher[i]);
    }
    fmpz_mod_poly_clear(in->c, *encrypt_modulus_ctx(encryption_scheme));
    for (int i = 0; i < VECTOR; i++) {
        fmpz_mod_poly_clear(in->u[i], *encrypt_modulus_ctx(encryption_scheme));
        for (int j = 0; j < DIM; j++) {
            for (int k = 0; k < 2; ++k) {
                fmpz_mod_poly_clear(in->r[i][j][k], *encrypt_modulus_ctx(encryption_scheme));
                fmpz_mod_poly_clear(in->e[i][j][k], *encrypt_modulus_ctx(encryption_scheme));
            }
        }

        for (int j = 0; j < 2; ++j) {
            fmpz_mod_poly_clear(in->e_[i][j], *encrypt_modulus_ctx(encryption_scheme));
        }
    }



}

#ifdef MAIN
// Tests and benchmarks below.
static void test(encryption_scheme_t encryption_scheme, flint_rand_t rand) {
	publickey_t pk;
	privatekey_t sk;
	veritext_t cipher;
	fmpz_mod_poly_t tmp, t[VECTOR], c, u, v;
	fmpz_mod_poly_t m[VECTOR], _m[VECTOR];

	fmpz_mod_poly_init(tmp, *encrypt_modulus_ctx(encryption_scheme));
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_init(m[i], *encrypt_modulus_ctx(encryption_scheme));
		fmpz_mod_poly_init(_m[i], *encrypt_modulus_ctx(encryption_scheme));
		encrypt_sample_short(m[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_init(t[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	fmpz_mod_poly_init(c, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_init(u, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_init(v, *encrypt_modulus_ctx(encryption_scheme));

	encrypt_keygen(encryption_scheme, &pk, &sk, rand);

	TEST_BEGIN("verifiable encryption is consistent") {
		for (int i = 0; i < VECTOR; i++) {
			encrypt_sample_short(m[i], *encrypt_modulus_ctx(encryption_scheme));
		}

		fmpz_mod_poly_zero(u, *encrypt_modulus_ctx(encryption_scheme));
		for (int i = 0; i < VECTOR; i++) {
			fmpz_mod_poly_randtest(t[i], rand, DEGREE, *encrypt_modulus_ctx(encryption_scheme));
			fmpz_mod_poly_mulmod(tmp, t[i], m[i], *encrypt_poly(encryption_scheme),
					*encrypt_modulus_ctx(encryption_scheme));
			fmpz_mod_poly_add(u, u, tmp, *encrypt_modulus_ctx(encryption_scheme));
		}

		TEST_ASSERT(vericrypt_doit(&cipher, t, u, m, encryption_scheme, &pk, rand) == 1, end);
		TEST_ASSERT(vericrypt_verify(&cipher, t, u, encryption_scheme, &pk) == 1, end);
		TEST_ASSERT(vericrypt_undo(_m, c, &cipher, t, u, encryption_scheme, &pk, &sk) == 1, end);

		fmpz_mod_poly_zero(v, *encrypt_modulus_ctx(encryption_scheme));
		for (int i = 0; i < VECTOR; i++) {
			fmpz_mod_poly_mulmod(tmp, t[i], _m[i], *encrypt_poly(encryption_scheme),
					*encrypt_modulus_ctx(encryption_scheme));
			fmpz_mod_poly_add(v, v, tmp, *encrypt_modulus_ctx(encryption_scheme));
		}
		fmpz_mod_poly_mulmod(u, u, c, *encrypt_poly(encryption_scheme), *encrypt_modulus_ctx(encryption_scheme));

		TEST_ASSERT(fmpz_mod_poly_equal(u, v, *encrypt_modulus_ctx(encryption_scheme)) == 1, end);
	} TEST_END;

  end:
	encrypt_keyfree(encryption_scheme, &pk, &sk);

	fmpz_mod_poly_clear(tmp, *encrypt_modulus_ctx(encryption_scheme));
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_clear(m[i], *encrypt_modulus_ctx(encryption_scheme));
		fmpz_mod_poly_clear(_m[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_clear(t[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	fmpz_mod_poly_clear(c, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_clear(u, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_clear(v, *encrypt_modulus_ctx(encryption_scheme));
}

static void bench(encryption_scheme_t encryption_scheme, flint_rand_t rand) {
	publickey_t pk;
	privatekey_t sk;
	veritext_t cipher;
	fmpz_mod_poly_t tmp, t[VECTOR], u, c;
	fmpz_mod_poly_t m[VECTOR], _m[VECTOR];

	fmpz_mod_poly_init(tmp, *encrypt_modulus_ctx(encryption_scheme));
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_init(m[i], *encrypt_modulus_ctx(encryption_scheme));
		fmpz_mod_poly_init(_m[i], *encrypt_modulus_ctx(encryption_scheme));
		encrypt_sample_short(m[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_init(t[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	fmpz_mod_poly_init(c, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_init(u, *encrypt_modulus_ctx(encryption_scheme));

	encrypt_keygen(encryption_scheme, &pk, &sk, rand);

	fmpz_mod_poly_zero(u, *encrypt_modulus_ctx(encryption_scheme));
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_randtest(t[i], rand, DEGREE, *encrypt_modulus_ctx(encryption_scheme));
		fmpz_mod_poly_mulmod(tmp, t[i], m[i], *encrypt_poly(encryption_scheme),
				*encrypt_modulus_ctx(encryption_scheme));
		fmpz_mod_poly_add(u, u, tmp, *encrypt_modulus_ctx(encryption_scheme));
	}

	BENCH_BEGIN("vericrypt_doit") {
		BENCH_ADD(vericrypt_doit(&cipher, t, u, m, encryption_scheme, &pk, rand));
	} BENCH_END;

	BENCH_BEGIN("vericrypt_verify") {
		BENCH_ADD(vericrypt_verify(&cipher, t, u, encryption_scheme, &pk));
	} BENCH_END;

	BENCH_BEGIN("vericrypt_sample_gauss") {
		BENCH_ADD(vericrypt_sample_gauss(tmp, encrypt_large_modulus_ctx(encryption_scheme)));
	} BENCH_END;

	BENCH_BEGIN("vericrypt_undo") {
		BENCH_ADD(vericrypt_undo(_m, c, &cipher, t, u, encryption_scheme, &pk, &sk));
	} BENCH_END;

	encrypt_keyfree(encryption_scheme, &pk, &sk);

	fmpz_mod_poly_clear(tmp, *encrypt_modulus_ctx(encryption_scheme));
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_clear(m[i], *encrypt_modulus_ctx(encryption_scheme));
		fmpz_mod_poly_clear(_m[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	for (int i = 0; i < VECTOR; i++) {
		fmpz_mod_poly_clear(t[i], *encrypt_modulus_ctx(encryption_scheme));
	}
	fmpz_mod_poly_clear(c, *encrypt_modulus_ctx(encryption_scheme));
	fmpz_mod_poly_clear(u, *encrypt_modulus_ctx(encryption_scheme));
}

int main(int argc, char *argv[]) {
	flint_rand_t rand;
    encryption_scheme_t encryption_scheme;

	flint_randinit(rand);
	encrypt_setup(encryption_scheme);

	printf("\n** Tests for lattice-based verifiable encryption:\n\n");
	test(encryption_scheme, rand);

	printf("\n** Benchmarks for lattice-based verifiable encryption:\n\n");
	bench(encryption_scheme, rand);

	encrypt_finish(encryption_scheme);
}

#endif
