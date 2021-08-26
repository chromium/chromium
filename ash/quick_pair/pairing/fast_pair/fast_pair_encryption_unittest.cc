// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_encryption.h"

#include <algorithm>
#include <array>
#include <iterator>

#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "base/base64.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

std::array<uint8_t, kBlockByteSize> aes_key_bytes = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

std::string DecodeKey(const std::string& encoded_key) {
  std::string key;
  base::Base64Decode(encoded_key, &key);
  return key;
}

class FastPairEncryptionTest : public testing::Test {};

TEST_F(FastPairEncryptionTest, EncryptBytes_Success) {
  std::array<uint8_t, kBlockByteSize> input = {
      0xF3, 0x0F, 0x4E, 0x78, 0x6C, 0x59, 0xA7, 0xBB,
      0xF3, 0x87, 0x3B, 0x5A, 0x49, 0xBA, 0x97, 0xEA};

  std::array<uint8_t, kBlockByteSize> expected = {
      0xAC, 0x9A, 0x16, 0xF0, 0x95, 0x3A, 0x3F, 0x22,
      0x3D, 0xD1, 0x0C, 0xF5, 0x36, 0xE0, 0x9E, 0x9C};

  EXPECT_EQ(EncryptBytes(aes_key_bytes, input), expected);
}

TEST_F(FastPairEncryptionTest, EncryptBytes_Failure) {
  std::array<uint8_t, kBlockByteSize> input = {
      0xF3, 0x0F, 0x4E, 0x78, 0x6C, 0x59, 0xA7, 0xBA,
      0xF3, 0x87, 0x3B, 0x5A, 0x49, 0xBA, 0x97, 0xEA};

  std::array<uint8_t, kBlockByteSize> expected = {
      0xAC, 0x9A, 0x16, 0xF0, 0x95, 0x3A, 0x3F, 0x22,
      0x3D, 0xD1, 0x0C, 0xF5, 0x36, 0xE0, 0x9E, 0x9C};

  EXPECT_NE(EncryptBytes(aes_key_bytes, input), expected);
}

TEST_F(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_EmptyKey) {
  EXPECT_FALSE(GenerateKeysWithEcdhKeyAgreement("").has_value());
}

TEST_F(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_ShortKey) {
  EXPECT_FALSE(GenerateKeysWithEcdhKeyAgreement("too_short").has_value());
}

TEST_F(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_InvalidKey) {
  EXPECT_FALSE(
      GenerateKeysWithEcdhKeyAgreement(
          DecodeKey("U2PWc3FHTxah/o0YT9n1VRvtm57SNIRSXOEBXm4fdtMo+06tNoFlt8D0/"
                    "2BsN8auolz5ikwLRvQh+MiQ6oYveg=="))
          .has_value());
}

TEST_F(FastPairEncryptionTest, GenerateKeysWithEcdhKeyAgreement_ValidKey) {
  EXPECT_TRUE(
      GenerateKeysWithEcdhKeyAgreement(
          DecodeKey("U2PWc3FHTxah/o0YU9n1VRvtm57SNIRSXOEBXm4fdtMo+06tNoFlt8D0/"
                    "2BsN8auolz5ikwLRvQh+MiQ6oYveg=="))
          .has_value());
}

TEST_F(FastPairEncryptionTest, ParseDecryptedResponse_Success) {
  std::vector<uint8_t> response_bytes;

  // Message type.
  response_bytes.push_back(0x01);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  std::copy(address_bytes.begin(), address_bytes.end(),
            std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x0E, 0x0F, 0x00};
  std::copy(salt.begin(), salt.end(), std::back_inserter(response_bytes));

  std::array<uint8_t, kBlockByteSize> response_bytes_array;
  std::copy_n(response_bytes.begin(), kBlockByteSize,
              response_bytes_array.begin());

  auto encrypted_bytes = EncryptBytes(aes_key_bytes, response_bytes_array);
  auto response = ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(response->message_type,
            FastPairMessageType::kKeyBasedPairingResponse);
  EXPECT_EQ(response->address_bytes, address_bytes);
  EXPECT_EQ(response->salt, salt);
}

TEST_F(FastPairEncryptionTest, ParseDecryptedResponse_Failure) {
  std::array<uint8_t, kBlockByteSize> response_bytes = {/*message_type=*/0x02,
                                                        /*address_bytes=*/0x02,
                                                        0x03,
                                                        0x04,
                                                        0x05,
                                                        0x06,
                                                        0x07,
                                                        /*salt=*/0x08,
                                                        0x09,
                                                        0x0A,
                                                        0x0B,
                                                        0x0C,
                                                        0x0D,
                                                        0x0E,
                                                        0x0F,
                                                        0x00};

  auto encrypted_bytes = EncryptBytes(aes_key_bytes, response_bytes);
  auto response = ParseDecryptedResponse(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(response.has_value());
}

TEST_F(FastPairEncryptionTest, ParseDecryptedPasskey_Success) {
  std::vector<uint8_t> passkey_bytes;

  // Message type.
  passkey_bytes.push_back(0x02);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  std::copy(salt.begin(), salt.end(), std::back_inserter(passkey_bytes));

  std::array<uint8_t, kBlockByteSize> passkey_bytes_array;
  std::copy_n(passkey_bytes.begin(), kBlockByteSize,
              passkey_bytes_array.begin());

  auto encrypted_bytes = EncryptBytes(aes_key_bytes, passkey_bytes_array);
  auto decrypted_passkey =
      ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_TRUE(decrypted_passkey.has_value());
  EXPECT_EQ(decrypted_passkey->message_type,
            FastPairMessageType::kSeekersPasskey);
  EXPECT_EQ(decrypted_passkey->passkey, passkey);
  EXPECT_EQ(decrypted_passkey->salt, salt);
}

TEST_F(FastPairEncryptionTest, ParseDecryptedPasskey_Failure) {
  std::array<uint8_t, kBlockByteSize> passkey_bytes = {/*message_type=*/0x04,
                                                       /*passkey=*/0x02,
                                                       0x03,
                                                       0x04,
                                                       /*salt=*/0x05,
                                                       0x06,
                                                       0x07,
                                                       0x08,
                                                       0x09,
                                                       0x0A,
                                                       0x0B,
                                                       0x0C,
                                                       0x0D,
                                                       0x0E,
                                                       0x0F,
                                                       0x0E};

  auto encrypted_bytes = EncryptBytes(aes_key_bytes, passkey_bytes);
  auto passkey = ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes);

  EXPECT_FALSE(passkey.has_value());
}

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
