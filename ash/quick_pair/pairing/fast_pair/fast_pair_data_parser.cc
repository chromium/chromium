// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_parser.h"

#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace {

constexpr int kDecryptedResponseMessageTypeIndex = 0;
constexpr int kDecryptedResponseAddressStartIndex = 1;
constexpr int kDecryptedResponseAddressEndIndex = 7;
constexpr int kDecryptedResponseSaltStartIndex = 7;
constexpr uint8_t kKeyBasedPairingResponseType = 0x01;

}  // namespace

namespace ash {
namespace quick_pair {

FastPairDataParser::FastPairDataParser() {
  crypto::EnsureOpenSSLInit();
}

FastPairDataParser::~FastPairDataParser() = default;

absl::optional<DecryptedResponse> FastPairDataParser::ParseDecryptedResponse(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kEncryptedResponseByteSize>&
        encrypted_response_bytes) {
  std::array<uint8_t, kEncryptedResponseByteSize> decrypted_response_bytes =
      DecryptBytes(aes_key_bytes, encrypted_response_bytes);

  uint8_t message_type =
      decrypted_response_bytes[kDecryptedResponseMessageTypeIndex];

  // If the message type index is not the expected fast pair message type, then
  // this is not a valid fast pair response.
  if (message_type != kKeyBasedPairingResponseType) {
    return absl::nullopt;
  }

  std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  static_assert(
      kDecryptedResponseAddressEndIndex - kDecryptedResponseAddressStartIndex ==
          kDecryptedResponseAddressByteSize,
      "");
  std::copy(
      decrypted_response_bytes.begin() + kDecryptedResponseAddressStartIndex,
      decrypted_response_bytes.begin() + kDecryptedResponseAddressEndIndex,
      address_bytes.begin());

  std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
  static_assert(kEncryptedResponseByteSize - kDecryptedResponseSaltStartIndex ==
                    kDecryptedResponseSaltByteSize,
                "");
  std::copy(decrypted_response_bytes.begin() + kDecryptedResponseSaltStartIndex,
            decrypted_response_bytes.end(), salt.begin());
  return DecryptedResponse(message_type, address_bytes, salt);
}

std::array<uint8_t, kEncryptedResponseByteSize>
FastPairDataParser::DecryptBytes(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kEncryptedResponseByteSize>&
        encrypted_response_bytes) {
  AES_KEY aes_key;
  AES_set_decrypt_key(aes_key_bytes.data(), aes_key_bytes.size() * 8, &aes_key);
  std::array<uint8_t, kEncryptedResponseByteSize> decrypted_response_bytes;
  static_assert(kEncryptedResponseByteSize == AES_BLOCK_SIZE, "");
  AES_decrypt(encrypted_response_bytes.data(), decrypted_response_bytes.data(),
              &aes_key);
  return decrypted_response_bytes;
}

}  // namespace quick_pair
}  // namespace ash
