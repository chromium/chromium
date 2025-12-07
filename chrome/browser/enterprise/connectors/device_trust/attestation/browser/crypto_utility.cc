// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/crypto_utility.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace enterprise_connectors {

namespace CryptoUtility {

namespace {

const unsigned int kWellKnownExponent = 65537;

unsigned char* StringAsOpenSSLBuffer(std::string* s) {
  return reinterpret_cast<unsigned char*>(std::data(*s));
}

}  // namespace

bssl::UniquePtr<RSA> GetRSA(const std::string& public_key_modulus_hex) {
  bssl::UniquePtr<RSA> rsa(RSA_new());
  bssl::UniquePtr<BIGNUM> n(BN_new());
  bssl::UniquePtr<BIGNUM> e(BN_new());
  if (!rsa || !e || !n) {
    LOG(ERROR) << __func__ << ": Failed to allocate RSA or BIGNUMs.";
    return nullptr;
  }
  BIGNUM* pn = n.get();
  if (!BN_set_word(e.get(), kWellKnownExponent) ||
      !BN_hex2bn(&pn, public_key_modulus_hex.c_str())) {
    LOG(ERROR) << __func__ << ": Failed to generate exponent or modulus.";
    return nullptr;
  }
  if (!RSA_set0_key(rsa.get(), n.release(), e.release(), nullptr)) {
    LOG(ERROR) << __func__ << ": Failed to set exponent or modulus.";
    return nullptr;
  }
  return rsa;
}

bool WrapKeyOAEP(base::span<const uint8_t> key,
                 RSA* wrapping_key,
                 const std::string& wrapping_key_id,
                 EncryptedData* output) {
  std::string encrypted_key;

  if (!wrapping_key) {
    LOG(ERROR) << "No wrapping_key.";
    return false;
  }

  encrypted_key.resize(RSA_size(wrapping_key));
  unsigned char* encrypted_key_buffer = StringAsOpenSSLBuffer(&encrypted_key);
  int length = RSA_public_encrypt(key.size(), key.data(), encrypted_key_buffer,
                                  wrapping_key, RSA_PKCS1_OAEP_PADDING);
  if (length == -1) {
    LOG(ERROR) << "RSA_public_encrypt failed.";
    return false;
  }
  encrypted_key.resize(length);
  output->set_wrapped_key(encrypted_key);
  output->set_wrapping_key_id(wrapping_key_id);
  return true;
}

}  // namespace CryptoUtility

}  // namespace enterprise_connectors
