// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/crypto_utility.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/encryptor.h"
#include "crypto/random.h"
#include "crypto/signature_verifier.h"
#include "crypto/symmetric_key.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace enterprise_connectors {

namespace CryptoUtility {

namespace {

const unsigned int kWellKnownExponent = 65537;
const size_t kAesKeySizeBits = 256;
const size_t kAesBlockSize = 16;

const unsigned char* StringAsConstOpenSSLBuffer(const std::string& s) {
  return reinterpret_cast<const unsigned char*>(s.data());
}

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

bool CreatePubKeyFromHex(const std::string& public_key_modulus_hex,
                         std::vector<uint8_t>& public_key_info) {
  bssl::UniquePtr<RSA> rsa = GetRSA(public_key_modulus_hex);

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

bool VerifySignatureUsingHexKey(const std::string& public_key_modulus_hex,
                                const std::string& data,
                                const std::string& signature) {
  std::vector<uint8_t> public_key_info;
  if (!CreatePubKeyFromHex(public_key_modulus_hex, public_key_info)) {
    return false;
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA256,
                           base::as_bytes(base::make_span(signature)),
                           base::as_bytes(base::make_span(public_key_info)))) {
    return false;
  }

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  return verifier.VerifyFinal();
}

std::string HmacSha512(const std::string& key, const std::string& data) {
  unsigned char mac[SHA512_DIGEST_LENGTH];
  std::string mutable_data(data);
  unsigned char* data_buffer = StringAsOpenSSLBuffer(&mutable_data);
  HMAC(EVP_sha512(), key.data(), key.size(), data_buffer, data.size(), mac,
       nullptr);
  return std::string(std::begin(mac), std::end(mac));
}

bool EncryptWithSeed(const std::string& data,
                     EncryptedData* encrypted,
                     std::string& key) {
  // Generate AES key of size 256 bits.
  std::unique_ptr<crypto::SymmetricKey> symmetric_key(
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES,
                                              kAesKeySizeBits));
  if (!symmetric_key) {
    return false;
  }
  key = symmetric_key->key();

  // Generate initialized vector of size 128 bits.
  std::string iv(kAesBlockSize, '\0');
  crypto::RandBytes(base::as_writable_byte_span(iv));

  crypto::Encryptor encryptor;
  if (!encryptor.Init(symmetric_key.get(), crypto::Encryptor::CBC, iv)) {
    return false;
  }
  if (!encryptor.Encrypt(data, encrypted->mutable_encrypted_data())) {
    return false;
  }

  encrypted->set_iv(iv);
  encrypted->set_mac(HmacSha512(key, iv + encrypted->encrypted_data()));
  return true;
}

bool WrapKeyOAEP(const std::string& key,
                 RSA* wrapping_key,
                 const std::string& wrapping_key_id,
                 EncryptedData* output) {
  const unsigned char* key_buffer = StringAsConstOpenSSLBuffer(key);
  std::string encrypted_key;

  if (!wrapping_key) {
    LOG(ERROR) << "No wrapping_key.";
    return false;
  }

  encrypted_key.resize(RSA_size(wrapping_key));
  unsigned char* encrypted_key_buffer = StringAsOpenSSLBuffer(&encrypted_key);
  int length = RSA_public_encrypt(key.size(), key_buffer, encrypted_key_buffer,
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
