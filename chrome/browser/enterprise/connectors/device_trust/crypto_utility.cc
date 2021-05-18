// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/crypto_utility.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace enterprise_connector {

namespace {

const unsigned int kWellKnownExponent = 65537;

bool CreatePublicKeyFromHex(const std::string& public_key_modulus_hex,
                            std::vector<uint8_t>& public_key_info) {
  bssl::UniquePtr<RSA> rsa(RSA_new());
  bssl::UniquePtr<BIGNUM> n(BN_new());
  bssl::UniquePtr<BIGNUM> e(BN_new());
  if (!rsa || !e || !n) {
    LOG(ERROR) << __func__ << ": Failed to allocate RSA or BIGNUMs.";
    return false;
  }
  BIGNUM* pn = n.get();
  if (!BN_set_word(e.get(), kWellKnownExponent) ||
      !BN_hex2bn(&pn, public_key_modulus_hex.c_str())) {
    LOG(ERROR) << __func__ << ": Failed to generate exponent or modulus.";
    return false;
  }
  if (!RSA_set0_key(rsa.get(), n.release(), e.release(), nullptr)) {
    LOG(ERROR) << __func__ << ": Failed to set exponent or modulus.";
    return false;
  }

  bssl::UniquePtr<EVP_PKEY> public_key(EVP_PKEY_new());
  EVP_PKEY_assign_RSA(public_key.get(), rsa.release());

  uint8_t* der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), public_key.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  public_key_info.assign(der, der + der_len);
  OPENSSL_free(der);

  return true;
}

}  // namespace

// static
bool CryptoUtility::VerifySignatureUsingHexKey(
    const std::string& public_key_modulus_hex,
    const std::string& data,
    const std::string& signature) {
  std::vector<uint8_t> public_key_info;
  if (!CreatePublicKeyFromHex(public_key_modulus_hex, public_key_info)) {
    return false;
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA256,
                           base::as_bytes(base::make_span(signature)),
                           base::as_bytes(base::make_span(public_key_info))))
    return false;

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  return verifier.VerifyFinal();
}

}  // namespace enterprise_connector
