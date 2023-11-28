// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>

#include "base/functional/callback.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"

inline constexpr int kBlockSizeBytes = 16;
constexpr int kNonceSizeBytes = 8;
constexpr uint8_t kHmacVerifyLenBytes = 8;

namespace ash {
namespace quick_pair {

constexpr int kHmacAdditionalDataPacketSizeBytes = 8;

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptor {
 public:
  // Encrypts bytes with the stored secret key.
  virtual const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) = 0;

  virtual const std::optional<std::array<uint8_t, 64>>& GetPublicKey() = 0;

  // Decrypt and parse decrypted response bytes with the stored secret key.
  virtual void ParseDecryptedResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      base::OnceCallback<void(const std::optional<DecryptedResponse>&)>
          callback) = 0;

  // Decrypt and parse decrypted passkey bytes with the stored secret key.
  virtual void ParseDecryptedPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      base::OnceCallback<void(const std::optional<DecryptedPasskey>&)>
          callback) = 0;

  // Creates data packet to write to GATT Additional Data Characteristic
  // according to the Fast Pair spec:
  // https://developers.google.com/nearby/fast-pair/specifications/characteristics#AdditionalData
  virtual std::vector<uint8_t> CreateAdditionalDataPacket(
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& additional_data) = 0;

  // Verifies the authenticity of `encrypted_additional_data` by generating an
  // HMAC SHA-256 hash and comparing it to `hmacSha256`.
  virtual bool VerifyEncryptedAdditionalData(
      const std::array<uint8_t, kHmacVerifyLenBytes> hmacSha256First8Bytes,
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& encrypted_additional_data) = 0;

  // Encrypts `additional_data` according to the Fast Pair spec:
  // https://developers.google.com/nearby/fast-pair/specifications/characteristics#AdditionalData
  virtual std::vector<uint8_t> EncryptAdditionalDataWithSecretKey(
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& additional_data) = 0;

  virtual ~FastPairDataEncryptor() = default;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
