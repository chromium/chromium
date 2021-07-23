// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>

#include "ash/quick_pair/pairing/fast_pair/fast_pair_encryption.h"

#include "ash/quick_pair/pairing/fast_pair/fast_pair_key_pair.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace {

// Converts the public anti-spoofing key into an EC_Point.
bssl::UniquePtr<EC_POINT> GetEcPointFromPublicAntiSpoofingKey(
    const bssl::UniquePtr<EC_GROUP>& ec_group,
    const std::string& decoded_public_anti_spoofing) {
  std::array<uint8_t, kPublicKeyByteSize + 1> buffer;
  buffer[0] = POINT_CONVERSION_UNCOMPRESSED;
  std::copy(decoded_public_anti_spoofing.begin(),
            decoded_public_anti_spoofing.end(), buffer.begin() + 1);

  bssl::UniquePtr<EC_POINT> new_ec_point(EC_POINT_new(ec_group.get()));
  EC_POINT_oct2point(ec_group.get(), new_ec_point.get(), buffer.data(),
                     buffer.size(), nullptr);

  return new_ec_point;
}

// Key derivation function to be used in hashing the generated secret key.
void* KDF(const void* in, size_t inlen, void* out, size_t* outlen) {
  // Set this to 16 since that's the amount of bytes we want to use
  // for the key, even though more will be written by SHA256 below.
  *outlen = kPrivateKeyByteSize;
  return SHA256(static_cast<const uint8_t*>(in), inlen,
                static_cast<uint8_t*>(out));
}

}  // namespace

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

KeyPair GenerateKeysWithEcdhKeyAgreement(
    const std::string& decoded_public_anti_spoofing) {
  // Generate the secp256r1 key-pair.
  bssl::UniquePtr<EC_GROUP> ec_group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  EC_KEY_generate_key(ec_key.get());

  // The ultimate goal here is to get a 64-byte public key. We accomplish this
  // by converting the generated public key into the uncompressed X9.62 format,
  // which is 0x04 followed by padded x and y coordinates.
  std::array<uint8_t, kPublicKeyByteSize + 1> uncompressed_private_key;
  EC_POINT_point2oct(ec_group.get(), EC_KEY_get0_public_key(ec_key.get()),
                     POINT_CONVERSION_UNCOMPRESSED,
                     uncompressed_private_key.data(),
                     uncompressed_private_key.size(), nullptr);

  // Generate the secret for use during encryption. Cannot use std::array
  // because size is determined by variable |secret_len|.
  size_t secret_len = (EC_GROUP_get_degree(ec_group.get()) + 7) / 8;
  uint8_t secret[secret_len];

  ECDH_compute_key(secret, secret_len,
                   GetEcPointFromPublicAntiSpoofingKey(
                       ec_group, decoded_public_anti_spoofing)
                       .get(),
                   ec_key.get(), &KDF);

  // Ensure that the secret is 16 bytes. Cannot use std::copy since |secret| is
  // a variably modified type, thus template is not fixed at compile time.
  std::array<uint8_t, kPrivateKeyByteSize> private_key;
  for (int i = 0; i < kPrivateKeyByteSize; ++i)
    private_key[i] = secret[i];

  // Ignore the first byte since it is 0x04, from the above uncompressed X9.62
  // format.
  std::array<uint8_t, kPublicKeyByteSize> public_key;
  std::copy(uncompressed_private_key.begin() + 1,
            uncompressed_private_key.end(), public_key.begin());

  return KeyPair(private_key, public_key);
}

const std::array<uint8_t, kBlockByteSize> EncryptBytes(
    const std::array<uint8_t, kBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kBlockByteSize>& bytes_to_encrypt) {
  AES_KEY aes_key;
  AES_set_encrypt_key(aes_key_bytes.data(), aes_key_bytes.size() * 8, &aes_key);
  std::array<uint8_t, kBlockByteSize> encrypted_bytes;
  AES_encrypt(bytes_to_encrypt.data(), encrypted_bytes.data(), &aes_key);
  return encrypted_bytes;
}

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
