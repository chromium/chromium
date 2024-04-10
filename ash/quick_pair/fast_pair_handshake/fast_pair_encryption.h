// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_ENCRYPTION_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_ENCRYPTION_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "ash/quick_pair/fast_pair_handshake/fast_pair_key_pair.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

constexpr int kBlockSizeBytes = 16;
constexpr int kHmacSizeBytes = 32;
constexpr int kHmacKeySizeBytes = 64;
constexpr int kNonceSizeBytes = 8;
constexpr int kSecretKeySizeBytes = 16;

std::optional<KeyPair> GenerateKeysWithEcdhKeyAgreement(
    const std::string& decoded_public_anti_spoofing);

const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
    const std::array<uint8_t, kBlockSizeBytes>& aes_key_bytes,
    const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt);

const std::array<uint8_t, kHmacSizeBytes> GenerateHmacSha256(
    const std::array<uint8_t, kSecretKeySizeBytes>& secret_key,
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& data);

// Encrypts `data` according to the Fast Pair Spec implementation of AES-CTR:
// https://developers.google.com/nearby/fast-pair/specifications/characteristics#AdditionalData.
// Notably used to encrypt data written to the GATT Additional Data
// characteristic.
const std::vector<uint8_t> EncryptAdditionalData(
    const std::array<uint8_t, kSecretKeySizeBytes>& secret_key,
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& data);

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_ENCRYPTION_H_
