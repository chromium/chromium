// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_challenge_wrapper.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/securemessage/proto/securemessage.pb.h"

namespace ash {
namespace {

const char kSalt[] =
    "\xbf\x9d\x2a\x53\xc6\x36\x16\xd7\x5d\xb0\xa7\x16\x5b\x91\xc1\xef\x73\xe5"
    "\x37\xf2\x42\x74\x05\xfa\x23\x61\x0a\x4b\xe6\x57\x64\x2e";

const char kChallenge[] = "challenge";
const char kChannelBindingData[] = "channel binding data";
const char kUserId[] = "user id";
const char kSignature[] = "signature";

void SaveResult(std::string* result_out, const std::string& result) {
  *result_out = result;
}

class TestableEasyUnlockChallengeWrapper : public EasyUnlockChallengeWrapper {
 public:
  TestableEasyUnlockChallengeWrapper()
      : EasyUnlockChallengeWrapper(kChallenge,
                                   kChannelBindingData,
                                   AccountId::FromUserEmail(kUserId),
                                   nullptr) {}

  TestableEasyUnlockChallengeWrapper(
      const TestableEasyUnlockChallengeWrapper&) = delete;
  TestableEasyUnlockChallengeWrapper& operator=(
      const TestableEasyUnlockChallengeWrapper&) = delete;

  ~TestableEasyUnlockChallengeWrapper() override {}

 private:
  void SignUsingTpmKey(
      const std::string& data_to_sign,
      base::OnceCallback<void(const std::string&)> callback) override {
    std::string expected_salt = std::string(kSalt);
    std::string expected_channel_binding_data =
        std::string(kChannelBindingData);

    std::string salt = data_to_sign.substr(0, expected_salt.length());
    std::string channel_binding_data = data_to_sign.substr(
        data_to_sign.length() - expected_channel_binding_data.length());
    std::string header_and_body = data_to_sign.substr(
        salt.length(),
        data_to_sign.length() - salt.length() - channel_binding_data.length());

    EXPECT_EQ(expected_salt, salt);
    EXPECT_EQ(expected_channel_binding_data, channel_binding_data);
    securemessage::HeaderAndBody proto;
    EXPECT_TRUE(proto.ParseFromString(header_and_body));

    std::move(callback).Run(kSignature);
  }
};

TEST(EasyUnlockChallengeWrapperTest, TestWrapChallenge) {
  TestableEasyUnlockChallengeWrapper wrapper;
  std::string wrapped_challenge;
  wrapper.WrapChallenge(base::BindOnce(&SaveResult, &wrapped_challenge));

  securemessage::SecureMessage challenge_secure_message;
  ASSERT_TRUE(challenge_secure_message.ParseFromString(wrapped_challenge));
  EXPECT_EQ(kChallenge, challenge_secure_message.header_and_body());

  securemessage::SecureMessage signature_secure_message;
  ASSERT_TRUE(signature_secure_message.ParseFromString(
      challenge_secure_message.signature()));
  EXPECT_EQ(kSignature, signature_secure_message.signature());
}

}  // namespace
}  // namespace ash
