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

const std::optional<std::array<uint8_t, 64>>&
FakeFastPairDataEncryptor::GetPublicKey() {
  return public_key_;
}

void FakeFastPairDataEncryptor::ParseDecryptedResponse(
    const std::vector<uint8_t>& encrypted_response_bytes,
    base::OnceCallback<void(const std::optional<DecryptedResponse>&)>
        callback) {
  std::move(callback).Run(response_);
}

void FakeFastPairDataEncryptor::ParseDecryptedPasskey(
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    base::OnceCallback<void(const std::optional<DecryptedPasskey>&)> callback) {
  std::move(callback).Run(passkey_);
}

std::vector<uint8_t> FakeFastPairDataEncryptor::CreateAdditionalDataPacket(
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& additional_data) {
  return additional_data_packet_encrypted_bytes_;
}

bool FakeFastPairDataEncryptor::VerifyEncryptedAdditionalData(
    const std::array<uint8_t, kHmacVerifyLenBytes> hmacSha256First8Bytes,
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& encrypted_additional_data) {
  return verify_;
}

std::vector<uint8_t>
FakeFastPairDataEncryptor::EncryptAdditionalDataWithSecretKey(
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& additional_data) {
  return encrypted_additional_data_;
}

}  // namespace quick_pair
}  // namespace ash
