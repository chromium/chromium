// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"

#include <algorithm>
#include <array>
#include <iterator>

#include "base/base64.h"
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

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
