// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "ash/quick_pair/pairing/fast_pair/fast_pair_encryption.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_key_pair.h"
#include "base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::array<uint8_t, kBlockSizeBytes> bytes_to_encrypt = {
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

const char kPublicAntiSpoof[] =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";

ash::quick_pair::fast_pair_encryption::KeyPair GetTestKeyPair() {
  std::string decoded_key;
  base::Base64Decode(kPublicAntiSpoof, &decoded_key);
  return ash::quick_pair::fast_pair_encryption::
      GenerateKeysWithEcdhKeyAgreement(decoded_key);
}

}  // namespace

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

class FastPairDataEncryptorTest : public testing::Test {
 public:
  void SetUp() override {
    KeyPair key_pair = GetTestKeyPair();
    private_key_ = key_pair.private_key;
    public_key_ = key_pair.public_key;
    data_encryptor_ = std::make_unique<FastPairDataEncryptor>(key_pair);
  }

  void TearDown() override {}

  FastPairDataEncryptor& data_encryptor() { return *(data_encryptor_.get()); }

  const std::array<uint8_t, kBlockByteSize> EncryptBytesWithPrivateKey() {
    return EncryptBytes(private_key_, bytes_to_encrypt);
  }

 private:
  std::array<uint8_t, kPrivateKeyByteSize> private_key_;
  std::array<uint8_t, kPublicKeyByteSize> public_key_;
  std::unique_ptr<FastPairDataEncryptor> data_encryptor_;
};

TEST_F(FastPairDataEncryptorTest, EncryptBytesUnsuccessfully) {
  EXPECT_EQ(data_encryptor().EncryptBytes(bytes_to_encrypt),
            EncryptBytesWithPrivateKey());
}

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
