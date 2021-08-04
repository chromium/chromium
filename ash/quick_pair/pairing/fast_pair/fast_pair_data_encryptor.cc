// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor.h"

#include "ash/quick_pair/pairing/fast_pair/fast_pair_encryption.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

FastPairDataEncryptor::FastPairDataEncryptor(const KeyPair& key_pair)
    : secret_key_(key_pair.private_key), public_key_(key_pair.public_key) {}

FastPairDataEncryptor::~FastPairDataEncryptor() = default;

const std::array<uint8_t, kBlockSizeBytes> FastPairDataEncryptor::EncryptBytes(
    const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) {
  return fast_pair_encryption::EncryptBytes(secret_key_, bytes_to_encrypt);
}

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
