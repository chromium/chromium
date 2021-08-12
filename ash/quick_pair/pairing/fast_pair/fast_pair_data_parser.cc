// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_parser.h"

#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "crypto/openssl_util.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace {

constexpr int kMessageTypeIndex = 0;
constexpr int kResponseAddressStartIndex = 1;
constexpr int kResponseSaltStartIndex = 7;
constexpr uint8_t kKeybasedPairingResponseType = 0x01;
constexpr uint8_t kSeekerPasskeyType = 0x02;
constexpr uint8_t kProviderPasskeyType = 0x03;
constexpr int kPasskeySaltStartIndex = 4;

}  // namespace

namespace ash {
namespace quick_pair {

FastPairDataParser::FastPairDataParser() {
  crypto::EnsureOpenSSLInit();
}

FastPairDataParser::~FastPairDataParser() = default;

absl::optional<DecryptedResponse> FastPairDataParser::ParseDecryptedResponse(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kEncryptedDataByteSize>&
        encrypted_response_bytes) {
  std::array<uint8_t, kEncryptedDataByteSize> decrypted_response_bytes =
      DecryptBytes(aes_key_bytes, encrypted_response_bytes);

  uint8_t message_type = decrypted_response_bytes[kMessageTypeIndex];

  // If the message type index is not the expected fast pair message type, then
  // this is not a valid fast pair response.
  if (message_type != kKeybasedPairingResponseType) {
    return absl::nullopt;
  }

  std::array<uint8_t, kDecryptedResponseAddressByteSize> address_bytes;
  static_assert(kResponseSaltStartIndex - kResponseAddressStartIndex ==
                    kDecryptedResponseAddressByteSize,
                "");
  std::copy(decrypted_response_bytes.begin() + kResponseAddressStartIndex,
            decrypted_response_bytes.begin() + kResponseSaltStartIndex,
            address_bytes.begin());

  std::array<uint8_t, kDecryptedResponseSaltByteSize> salt;
  static_assert(kEncryptedDataByteSize - kResponseSaltStartIndex ==
                    kDecryptedResponseSaltByteSize,
                "");
  std::copy(decrypted_response_bytes.begin() + kResponseSaltStartIndex,
            decrypted_response_bytes.end(), salt.begin());
  return DecryptedResponse(FastPairMessageType::kKeyBasedPairingResponse,
                           address_bytes, salt);
}

absl::optional<DecryptedPasskey> FastPairDataParser::ParseDecryptedPasskey(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kEncryptedDataByteSize>&
        encrypted_passkey_bytes) {
  std::array<uint8_t, kEncryptedDataByteSize> decrypted_passkey_bytes =
      DecryptBytes(aes_key_bytes, encrypted_passkey_bytes);

  FastPairMessageType message_type;
  if (decrypted_passkey_bytes[kMessageTypeIndex] == kSeekerPasskeyType) {
    message_type = FastPairMessageType::kSeekersPasskey;
  } else if (decrypted_passkey_bytes[kMessageTypeIndex] ==
             kProviderPasskeyType) {
    message_type = FastPairMessageType::kProvidersPasskey;
  } else {
    return absl::nullopt;
  }

  uint32_t passkey = decrypted_passkey_bytes[3];
  passkey += decrypted_passkey_bytes[2] << 8;
  passkey += decrypted_passkey_bytes[1] << 16;

  std::array<uint8_t, kDecryptedPasskeySaltByteSize> salt;
  static_assert(kEncryptedDataByteSize - kPasskeySaltStartIndex ==
                    kDecryptedPasskeySaltByteSize,
                "");
  std::copy(decrypted_passkey_bytes.begin() + kPasskeySaltStartIndex,
            decrypted_passkey_bytes.end(), salt.begin());
  return DecryptedPasskey(message_type, passkey, salt);
}

std::array<uint8_t, kEncryptedDataByteSize> FastPairDataParser::DecryptBytes(
    const std::array<uint8_t, kAesBlockByteSize>& aes_key_bytes,
    const std::array<uint8_t, kEncryptedDataByteSize>& encrypted_bytes) {
  AES_KEY aes_key;
  AES_set_decrypt_key(aes_key_bytes.data(), aes_key_bytes.size() * 8, &aes_key);
  std::array<uint8_t, kEncryptedDataByteSize> decrypted_bytes;
  static_assert(kEncryptedDataByteSize == AES_BLOCK_SIZE, "");
  AES_decrypt(encrypted_bytes.data(), decrypted_bytes.data(), &aes_key);
  return decrypted_bytes;
}

}  // namespace quick_pair
}  // namespace ash
