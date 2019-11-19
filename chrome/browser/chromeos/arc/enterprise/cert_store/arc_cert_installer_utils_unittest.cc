// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_installer_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace arc {

namespace {

// Keep in sync with external/boringssl/src/crypto/fipsmodule/rsa/rsa.c
int checkRsaKey(const RSA* key) {
  BIGNUM n, pm1, qm1, lcm, gcd, de, dmp1, dmq1, iqmp_times_q;
  BN_CTX* ctx;
  int ok = 0, has_crt_values;

  if (RSA_is_opaque(key)) {
    // Opaque keys can't be checked.
    return 1;
  }

  if ((key->p != nullptr) != (key->q != nullptr)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_ONLY_ONE_OF_P_Q_GIVEN);
    return 0;
  }

  if (!key->n || !key->e) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  if (!key->d || !key->p) {
    // For a public key, or without p and q, there's nothing that can be
    // checked.
    return 1;
  }

  ctx = BN_CTX_new();
  if (ctx == nullptr) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_MALLOC_FAILURE);
    return 0;
  }

  BN_init(&n);
  BN_init(&pm1);
  BN_init(&qm1);
  BN_init(&lcm);
  BN_init(&gcd);
  BN_init(&de);
  BN_init(&dmp1);
  BN_init(&dmq1);
  BN_init(&iqmp_times_q);

  if (!BN_mul(&n, key->p, key->q, ctx) ||
      // lcm = lcm(p, q)
      !BN_sub(&pm1, key->p, BN_value_one()) ||
      !BN_sub(&qm1, key->q, BN_value_one()) || !BN_mul(&lcm, &pm1, &qm1, ctx) ||
      !BN_gcd(&gcd, &pm1, &qm1, ctx)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    goto out;
  }

  if (!BN_div(&lcm, nullptr, &lcm, &gcd, ctx) ||
      !BN_gcd(&gcd, &pm1, &qm1, ctx) ||
      // de = d*e mod lcm(p, q).
      !BN_mod_mul(&de, key->d, key->e, &lcm, ctx)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    goto out;
  }

  if (BN_cmp(&n, key->n) != 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_N_NOT_EQUAL_P_Q);
    goto out;
  }

  if (!BN_is_one(&de)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_D_E_NOT_CONGRUENT_TO_1);
    goto out;
  }

  has_crt_values = key->dmp1 != nullptr;

  if (has_crt_values != (key->dmq1 != nullptr) ||
      has_crt_values != (key->iqmp != nullptr)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_INCONSISTENT_SET_OF_CRT_VALUES);
    goto out;
  }

  if (has_crt_values) {
    if (  // dmp1 = d mod (p-1)
        !BN_mod(&dmp1, key->d, &pm1, ctx) ||
        // dmq1 = d mod (q-1)
        !BN_mod(&dmq1, key->d, &qm1, ctx) ||
        // iqmp = q^-1 mod p
        !BN_mod_mul(&iqmp_times_q, key->iqmp, key->q, key->p, ctx)) {
      OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
      goto out;
    }

    if (BN_cmp(&dmp1, key->dmp1) != 0 || BN_cmp(&dmq1, key->dmq1) != 0 ||
        BN_cmp(key->iqmp, key->p) >= 0 || !BN_is_one(&iqmp_times_q)) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_CRT_VALUES_INCORRECT);
      goto out;
    }
  }

  ok = 1;

out:
  BN_free(&n);
  BN_free(&pm1);
  BN_free(&qm1);
  BN_free(&lcm);
  BN_free(&gcd);
  BN_free(&de);
  BN_free(&dmp1);
  BN_free(&dmq1);
  BN_free(&iqmp_times_q);
  BN_CTX_free(ctx);

  return ok;
}

}  // namespace

class ArcCertInstallerUtilsTest
    : public testing::Test,
      public testing::WithParamInterface<std::string> {};

// Test that CreatePkcs12FromBlob returns non-empty PKCS12 blob with a valid
// RSA private key.
TEST_P(ArcCertInstallerUtilsTest, Pkcs12) {
  const std::string name = GetParam();
  EXPECT_FALSE(CreatePkcs12FromBlob(name).empty());

  RSA* rsa = CreateRsaPrivateKeyFromBlob(name);
  ASSERT_TRUE(rsa);
  EXPECT_TRUE(checkRsaKey(rsa));
  RSA_free(rsa);
}

INSTANTIATE_TEST_SUITE_P(,
                         ArcCertInstallerUtilsTest,
                         testing::Values("",
                                         "name of the smart card",
                                         std::string(2048, 'A')));

}  // namespace arc
