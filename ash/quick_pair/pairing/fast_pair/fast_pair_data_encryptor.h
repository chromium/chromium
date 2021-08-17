// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_H_
#define ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

namespace {

constexpr int kBlockSizeBytes = 16;

}  // namespace

namespace ash {
namespace quick_pair {

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptor {
 public:
  // Encrypts bytes with the stored secret key.
  virtual const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) = 0;

  virtual ~FastPairDataEncryptor() = default;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_PAIRING_FAST_PAIR_FAST_PAIR_DATA_ENCRYPTOR_H_
