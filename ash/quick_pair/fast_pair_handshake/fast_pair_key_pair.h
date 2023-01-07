// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_KEY_PAIR_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_KEY_PAIR_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

inline constexpr int kPrivateKeyByteSize = 16;
inline constexpr int kPublicKeyByteSize = 64;

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

// Key pair structure to represent public and private keys used for encryption/
// decryption.
struct KeyPair {
  KeyPair(std::array<uint8_t, kPrivateKeyByteSize> private_key,
          std::array<uint8_t, kPublicKeyByteSize> public_key);
  KeyPair(const KeyPair&);
  KeyPair(KeyPair&&);
  KeyPair& operator=(const KeyPair&) = delete;
  KeyPair& operator=(KeyPair&&) = delete;
  ~KeyPair() = default;

  const std::array<uint8_t, kPrivateKeyByteSize> private_key;
  const std::array<uint8_t, kPublicKeyByteSize> public_key;
};

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAST_PAIR_KEY_PAIR_H_
