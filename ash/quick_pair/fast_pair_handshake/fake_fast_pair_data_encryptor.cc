// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"

namespace ash {
namespace quick_pair {

FakeFastPairDataEncryptor::FakeFastPairDataEncryptor() = default;

FakeFastPairDataEncryptor::~FakeFastPairDataEncryptor() = default;

const std::array<uint8_t, kBlockSizeBytes>
FakeFastPairDataEncryptor::EncryptBytes(
    const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) {
  return encrypted_bytes_;
}

const absl::optional<std::array<uint8_t, 64>>&
FakeFastPairDataEncryptor::GetPublicKey() {
  return public_key_;
}

void FakeFastPairDataEncryptor::ParseDecryptedResponse(
    const std::vector<uint8_t>& encrypted_response_bytes,
    base::OnceCallback<void(const absl::optional<DecryptedResponse>&)>
        callback) {
  std::move(callback).Run(response_);
}

void FakeFastPairDataEncryptor::ParseDecryptedPasskey(
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    base::OnceCallback<void(const absl::optional<DecryptedPasskey>&)>
        callback) {
  std::move(callback).Run(passkey_);
}

}  // namespace quick_pair
}  // namespace ash
