// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "ash/quick_pair/pairing/fast_pair/fast_pair_key_pair.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr int kBlockSizeBytes = 16;

}  // namespace

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptor {
 public:
  FastPairDataEncryptor(const KeyPair& key_pair);
  ~FastPairDataEncryptor();
  FastPairDataEncryptor(const FastPairDataEncryptor&) = delete;
  FastPairDataEncryptor& operator=(const FastPairDataEncryptor&) = delete;

  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt);

 private:
  std::array<uint8_t, kPrivateKeyByteSize> secret_key_;

  // The public key is only required during initial pairing and optional during
  // communication with paired devices.
  absl::optional<std::array<uint8_t, kPublicKeyByteSize>> public_key_ =
      absl::nullopt;
};

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_H_
