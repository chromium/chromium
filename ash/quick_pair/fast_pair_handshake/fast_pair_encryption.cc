// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"

#include <algorithm>
#include <array>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_key_pair.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace {

using ash::quick_pair::fast_pair_encryption::kBlockByteSize;

// Converts the public anti-spoofing key into an EC_Point.
bssl::UniquePtr<EC_POINT> GetEcPointFromPublicAntiSpoofingKey(
    const bssl::UniquePtr<EC_GROUP>& ec_group,
    const std::string& decoded_public_anti_spoofing) {
  std::array<uint8_t, kPublicKeyByteSize + 1> buffer;
  buffer[0] = POINT_CONVERSION_UNCOMPRESSED;
  base::ranges::copy(decoded_public_anti_spoofing, buffer.begin() + 1);

  bssl::UniquePtr<EC_POINT> new_ec_point(EC_POINT_new(ec_group.get()));

  if (!EC_POINT_oct2point(ec_group.get(), new_ec_point.get(), buffer.data(),
                          buffer.size(), nullptr)) {
    return nullptr;
  }

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

absl::optional<KeyPair> GenerateKeysWithEcdhKeyAgreement(
    const std::string& decoded_public_anti_spoofing) {
  if (decoded_public_anti_spoofing.size() != kPublicKeyByteSize) {
    QP_LOG(WARNING) << "Expected " << kPublicKeyByteSize
                    << " byte value for anti-spoofing key. Got:"
                    << decoded_public_anti_spoofing.size();
    return absl::nullopt;
  }

  // Generate the secp256r1 key-pair.
  bssl::UniquePtr<EC_GROUP> ec_group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

  if (!EC_KEY_generate_key(ec_key.get())) {
    QP_LOG(WARNING) << __func__ << ": Failed to generate ec key";
    return absl::nullopt;
  }

  // The ultimate goal here is to get a 64-byte public key. We accomplish this
  // by converting the generated public key into the uncompressed X9.62 format,
  // which is 0x04 followed by padded x and y coordinates.
  std::array<uint8_t, kPublicKeyByteSize + 1> uncompressed_private_key;
  int point_bytes_written = EC_POINT_point2oct(
      ec_group.get(), EC_KEY_get0_public_key(ec_key.get()),
      POINT_CONVERSION_UNCOMPRESSED, uncompressed_private_key.data(),
      uncompressed_private_key.size(), nullptr);

  if (point_bytes_written != uncompressed_private_key.size()) {
    QP_LOG(WARNING) << __func__
                    << ": EC_POINT_point2oct failed to convert public key to "
                       "uncompressed x9.62 format.";
    return absl::nullopt;
  }

  bssl::UniquePtr<EC_POINT> public_anti_spoofing_point =
      GetEcPointFromPublicAntiSpoofingKey(ec_group,
                                          decoded_public_anti_spoofing);

  if (!public_anti_spoofing_point) {
    QP_LOG(WARNING)
        << __func__
        << ": Failed to convert Public Anti-Spoofing key to EC_POINT";
    return absl::nullopt;
  }

  uint8_t secret[SHA256_DIGEST_LENGTH];
  int computed_key_size =
      ECDH_compute_key(secret, SHA256_DIGEST_LENGTH,
                       public_anti_spoofing_point.get(), ec_key.get(), &KDF);

  if (computed_key_size != kPrivateKeyByteSize) {
    QP_LOG(WARNING) << __func__ << ": ECDH_compute_key failed.";
    return absl::nullopt;
  }

  // Take first 16 bytes from secret as the private key.
  std::array<uint8_t, kPrivateKeyByteSize> private_key;
  std::copy(secret, secret + kPrivateKeyByteSize, std::begin(private_key));

  // Ignore the first byte since it is 0x04, from the above uncompressed X9 .62
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
  int aes_key_was_set = AES_set_encrypt_key(aes_key_bytes.data(),
                                            aes_key_bytes.size() * 8, &aes_key);
  DCHECK(aes_key_was_set == 0) << "Invalid AES key size.";
  std::array<uint8_t, kBlockByteSize> encrypted_bytes;
  AES_encrypt(bytes_to_encrypt.data(), encrypted_bytes.data(), &aes_key);
  return encrypted_bytes;
}

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
