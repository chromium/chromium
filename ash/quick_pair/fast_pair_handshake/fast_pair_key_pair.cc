// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_key_pair.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

KeyPair::KeyPair(std::array<uint8_t, kPrivateKeyByteSize> private_key,
                 std::array<uint8_t, kPublicKeyByteSize> public_key)
    : private_key(std::move(private_key)), public_key(std::move(public_key)) {}

KeyPair::KeyPair(const KeyPair&) = default;

KeyPair::KeyPair(KeyPair&&) = default;

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
