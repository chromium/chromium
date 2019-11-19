// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_installer_utils.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs8.h"

namespace arc {

// Creates a fake, but recognized as valid, RSA private key:
// * q = |blob| + 3 + { 4, 3, 2 } so that q % 3 = 1
// * p = 3
// * n = p * q
// * e = 1
// * d = (q - 1) * (p - 1) + 1 so that d % (p - 1) * (q - 1) = 1
// * dmp1 = 1
// * dmq1 = 1
// * iqmp = 1
RSA* CreateRsaPrivateKeyFromBlob(const std::string& blob) {
  const uint8_t* ptr = (const uint8_t*)blob.data();
  BIGNUM* q = BN_bin2bn(ptr, blob.size(), nullptr);
  int mod3;
  if (!q || !BN_add_word(q, 3) || (mod3 = BN_mod_word(q, 3)) >= 3 ||
      !BN_add_word(q, 4 - mod3)) {
    BN_free(q);
    VLOG(1) << "Failed to create q for " << blob;
    return nullptr;
  }

  BIGNUM* p = BN_new();
  if (!p || !BN_set_word(p, 3)) {
    BN_free(q);
    BN_free(p);
    VLOG(1) << "Failed to create p for " << blob;
    return nullptr;
  }

  BIGNUM* n = BN_new();
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (!n || !BN_mul(n, p, q, ctx.get())) {
    BN_free(q);
    BN_free(p);
    BN_free(n);
    VLOG(1) << "Failed to create n for " << blob;
    return nullptr;
  }

  BIGNUM* e = BN_new();
  if (!e || !BN_one(e)) {
    BN_free(q);
    BN_free(p);
    BN_free(n);
    BN_free(e);
    VLOG(1) << "Failed to create e for " << blob;
    return nullptr;
  }

  BIGNUM* d = BN_new();
  if (!d || !BN_sub(d, q, BN_value_one()) || !BN_mul_word(d, 2) ||
      !BN_add_word(d, 1)) {
    BN_free(q);
    BN_free(p);
    BN_free(n);
    BN_free(e);
    BN_free(d);
    VLOG(1) << "Failed to create d for " << blob;
    return nullptr;
  }

  BIGNUM* dmp1 = BN_new();
  BIGNUM* dmq1 = BN_new();
  BIGNUM* iqmp = BN_new();
  if (!dmp1 || !BN_one(dmp1) || !dmq1 || !BN_one(dmq1) || !iqmp ||
      !BN_one(iqmp)) {
    BN_free(q);
    BN_free(p);
    BN_free(n);
    BN_free(e);
    BN_free(d);
    BN_free(dmp1);
    BN_free(dmq1);
    BN_free(iqmp);
    VLOG(1) << "Failed to create dmp1 or dmq1 or iqmp for " << blob;
    return nullptr;
  }

  RSA* rsa = RSA_new();
  if (!rsa) {
    BN_free(q);
    BN_free(p);
    BN_free(n);
    BN_free(e);
    BN_free(d);
    BN_free(dmp1);
    BN_free(dmq1);
    BN_free(iqmp);
    VLOG(1) << "Failed to create RSA key.";
    return nullptr;
  }

  if (!RSA_set0_key(rsa, n, e, d)) {
    BN_free(q);
    BN_free(p);
    BN_free(n);
    BN_free(e);
    BN_free(d);
    BN_free(dmp1);
    BN_free(dmq1);
    BN_free(iqmp);
    RSA_free(rsa);
    VLOG(1) << "Failed to set n to RSA for " << blob;
    return nullptr;
  }

  if (!RSA_set0_factors(rsa, p, q)) {
    BN_free(q);
    BN_free(p);
    BN_free(dmp1);
    BN_free(dmq1);
    BN_free(iqmp);
    RSA_free(rsa);
    VLOG(1) << "Failed to set factors to RSA for " << blob;
    return nullptr;
  }

  if (!RSA_set0_crt_params(rsa, dmp1, dmq1, iqmp)) {
    BN_free(dmp1);
    BN_free(dmq1);
    BN_free(iqmp);
    RSA_free(rsa);
    VLOG(1) << "Failed to set crt params for " << blob;
    return nullptr;
  }
  return rsa;
}

std::string CreatePkcs12FromBlob(const std::string& blob) {
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());

  if (!pkey) {
    VLOG(1) << "Failed to generate RSA for " << blob;
    return "";
  }

  RSA* rsa = CreateRsaPrivateKeyFromBlob(blob);
  if (!rsa) {
    return "";
  }

  if (!EVP_PKEY_assign_RSA(pkey.get(), rsa)) {
    RSA_free(rsa);
    VLOG(1) << "Failed to assign RSA for " << blob;
    return "";
  }

  // Make a PKCS#12 blob.
  bssl::UniquePtr<PKCS12> pkcs12(PKCS12_create(
      nullptr, blob.c_str(), pkey.get(), nullptr, nullptr, 0, 0, 0, 0, 0));
  if (!pkcs12) {
    VLOG(1) << "Failed to create PKCS12 object from pkey for " << blob;
    return "";
  }

  uint8_t* key = nullptr;
  int key_len;
  if (!(key_len = i2d_PKCS12(pkcs12.get(), &key))) {
    VLOG(1) << "Failed to translate PKCS12 to byte array for " << blob;
    return "";
  }

  bssl::UniquePtr<uint8_t> free_key(key);

  std::string encoded_key;
  base::Base64Encode(base::StringPiece((char*)key, key_len), &encoded_key);
  return encoded_key;
}

}  // namespace arc
