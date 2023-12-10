// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/account_transfer_client_data.h"

#include "base/json/json_reader.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::quick_start {

namespace {
const char kChallengeBase64Url[] = "my string";
}  // namespace

class AccountTransferClientDataTest : public testing::Test {
 public:
  AccountTransferClientDataTest() = default;
  AccountTransferClientDataTest(AccountTransferClientDataTest&) = delete;
  AccountTransferClientDataTest& operator=(AccountTransferClientDataTest&) =
      delete;
  ~AccountTransferClientDataTest() override = default;

  Base64UrlString challenge_b64url_ = Base64UrlString(kChallengeBase64Url);
};

TEST_F(AccountTransferClientDataTest, CreateFidoAccountTransferClientDataJson) {
  url::Origin origin = url::Origin::Create(GURL(kOrigin));

  AccountTransferClientData data(challenge_b64url_);

  std::string client_data_json = data.CreateJson();
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(client_data_json);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  EXPECT_EQ(*parsed_json_dict.FindString(kClientDataTypeKey), kCtapRequestType);
  EXPECT_EQ(*parsed_json_dict.FindString(kClientDataChallengeKey),
            *Base64UrlString(kChallengeBase64Url));
  EXPECT_EQ(*parsed_json_dict.FindString(kClientDataOriginKey),
            origin.Serialize());
  EXPECT_FALSE(parsed_json_dict.FindBool(kClientDataCrossOriginKey).value());
}

TEST_F(AccountTransferClientDataTest, CreateHash) {
  AccountTransferClientData data(challenge_b64url_);

  std::string client_data_json = data.CreateJson();

  std::string json = data.CreateJson();
  std::array<uint8_t, crypto::kSHA256Length> expected_hash;
  crypto::SHA256HashString(json, expected_hash.data(), expected_hash.size());

  std::array<uint8_t, crypto::kSHA256Length> result = data.CreateHash();

  EXPECT_EQ(expected_hash, result);
}

}  // namespace ash::quick_start
