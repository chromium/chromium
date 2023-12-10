// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
#define ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_DATA_ENCRYPTOR_H_

#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"

namespace ash {
namespace quick_pair {

class FakeFastPairDataEncryptor : public FastPairDataEncryptor {
 public:
  FakeFastPairDataEncryptor();
  FakeFastPairDataEncryptor(const FakeFastPairDataEncryptor&) = delete;
  FakeFastPairDataEncryptor& operator=(const FakeFastPairDataEncryptor&) =
      delete;
  ~FakeFastPairDataEncryptor() override;

  void public_key(std::optional<std::array<uint8_t, 64>> public_key) {
    public_key_ = std::move(public_key);
  }

  void encrypted_bytes(std::array<uint8_t, kBlockSizeBytes> encrypted_bytes) {
    encrypted_bytes_ = std::move(encrypted_bytes);
  }

  void response(std::optional<DecryptedResponse> response) {
    response_ = std::move(response);
  }

  void passkey(std::optional<DecryptedPasskey> passkey) {
    passkey_ = std::move(passkey);
  }

  void additional_data_packet_encrypted_bytes(
      std::vector<uint8_t> additional_data_packet_encrypted_bytes) {
    additional_data_packet_encrypted_bytes_ =
        std::move(additional_data_packet_encrypted_bytes);
  }
  void encrypted_additional_data(
      std::vector<uint8_t> encrypted_additional_data) {
    encrypted_additional_data_ = std::move(encrypted_additional_data);
  }

  void verify_additional_data(bool verify) { verify_ = verify; }

  // FastPairDataEncryptor:
  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) override;
  const std::optional<std::array<uint8_t, 64>>& GetPublicKey() override;
  void ParseDecryptedResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      base::OnceCallback<void(const std::optional<DecryptedResponse>&)>
          callback) override;
  void ParseDecryptedPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      base::OnceCallback<void(const std::optional<DecryptedPasskey>&)> callback)
      override;
  std::vector<uint8_t> CreateAdditionalDataPacket(
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& additional_data) override;
  bool VerifyEncryptedAdditionalData(
      const std::array<uint8_t, kHmacVerifyLenBytes> hmacSha256First8Bytes,
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& encrypted_additional_data) override;
  std::vector<uint8_t> EncryptAdditionalDataWithSecretKey(
      std::array<uint8_t, kNonceSizeBytes> nonce,
      const std::vector<uint8_t>& additional_data) override;

 private:
  std::optional<std::array<uint8_t, 64>> public_key_ = std::nullopt;
  std::array<uint8_t, kBlockSizeBytes> encrypted_bytes_ = {};
  std::vector<uint8_t> additional_data_packet_encrypted_bytes_ = {};
  std::optional<DecryptedResponse> response_ = std::nullopt;
  std::optional<DecryptedPasskey> passkey_ = std::nullopt;
  std::vector<uint8_t> encrypted_additional_data_ = {};
  bool verify_ = false;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FAST_PAIR_HANDSHAKE_FAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
