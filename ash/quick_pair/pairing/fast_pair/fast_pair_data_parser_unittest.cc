// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_parser.h"

#include <stddef.h>

#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"

namespace {

std::array<uint8_t, kAesBlockByteSize> aes_key_bytes = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

std::array<uint8_t, kAesBlockByteSize> EncryptBytes(
    const std::array<uint8_t, kAesBlockByteSize>& bytes) {
  AES_KEY aes_key;
  AES_set_encrypt_key(aes_key_bytes.data(), aes_key_bytes.size() * 8, &aes_key);
  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes;
  AES_encrypt(bytes.data(), encrypted_bytes.data(), &aes_key);
  return encrypted_bytes;
}

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDataParserTest : public testing::Test {
 public:
  void SetUp() override {
    data_parser_ = std::make_unique<FastPairDataParser>();
  }

  void TearDown() override {}

  FastPairDataParser& data_parser() { return *(data_parser_.get()); }

 private:
  std::unique_ptr<FastPairDataParser> data_parser_;
};

TEST_F(FastPairDataParserTest, DecryptResponseUnsuccessfully) {
  std::array<uint8_t, kAesBlockByteSize> response_bytes = {
      /*message_type=*/0x02,
      /*address_bytes=*/0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      /*salt=*/0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes =
      EncryptBytes(response_bytes);

  absl::optional<DecryptedResponse> decrypted_response =
      data_parser().ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(decrypted_response.has_value());
}

TEST_F(FastPairDataParserTest, DecryptResponseSuccessfully) {
  std::array<uint8_t, kAesBlockByteSize> response_bytes;

  // Message type.
  uint8_t message_type = 0x01;
  response_bytes[0] = message_type;

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::copy(address_bytes.begin(), address_bytes.end(),
            response_bytes.begin() + 1);

  // Random salt
  std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B,
                                 0x0C, 0x0D, 0x0E, 0x0F};
  std::copy(salt.begin(), salt.end(), response_bytes.begin() + 7);

  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes =
      EncryptBytes(response_bytes);

  absl::optional<DecryptedResponse> decrypted_response =
      data_parser().ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(decrypted_response.has_value());
  EXPECT_EQ(decrypted_response->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
  EXPECT_EQ(decrypted_response->address_bytes, address_bytes);
  EXPECT_EQ(decrypted_response->salt, salt);
}

TEST_F(FastPairDataParserTest, DecryptPasskeyUnsuccessfully) {
  std::array<uint8_t, kAesBlockByteSize> passkey_bytes = {
    /*message_type=*/0x04,
    /*passkey=*/0x02, 0x03, 0x04,
    /*salt=*/0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
             0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x0E};
  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes =
      EncryptBytes(passkey_bytes);

  absl::optional<DecryptedPasskey> decrypted_passkey =
      data_parser().ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(decrypted_passkey.has_value());
}

TEST_F(FastPairDataParserTest, DecryptSeekerPasskeySuccessfully) {
  std::array<uint8_t, kAesBlockByteSize> passkey_bytes;
  // Message type.
  uint8_t message_type = 0x02;
  passkey_bytes[0] = message_type;

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes[1] = passkey >> 16;
  passkey_bytes[2] = passkey >> 8;
  passkey_bytes[3] = passkey;

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(salt.begin(), salt.end(), passkey_bytes.begin() + 4);

  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes =
      EncryptBytes(passkey_bytes);

  absl::optional<DecryptedPasskey> decrypted_passkey =
      data_parser().ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(decrypted_passkey.has_value());
  EXPECT_EQ(decrypted_passkey->message_type,
            FastPairMessageType::kSeekersPasskey);
  EXPECT_EQ(decrypted_passkey->passkey, passkey);
  EXPECT_EQ(decrypted_passkey->salt, salt);
}

TEST_F(FastPairDataParserTest, DecryptProviderPasskeySuccessfully) {
  std::array<uint8_t, kAesBlockByteSize> passkey_bytes;
  // Message type.
  uint8_t message_type = 0x03;
  passkey_bytes[0] = message_type;

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes[1] = passkey >> 16;
  passkey_bytes[2] = passkey >> 8;
  passkey_bytes[3] = passkey;

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(salt.begin(), salt.end(), passkey_bytes.begin() + 4);

  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes =
      EncryptBytes(passkey_bytes);

  absl::optional<DecryptedPasskey> decrypted_passkey =
      data_parser().ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(decrypted_passkey.has_value());
  EXPECT_EQ(decrypted_passkey->message_type,
            FastPairMessageType::kProvidersPasskey);
  EXPECT_EQ(decrypted_passkey->passkey, passkey);
  EXPECT_EQ(decrypted_passkey->salt, salt);
}

}  // namespace quick_pair
}  // namespace ash
