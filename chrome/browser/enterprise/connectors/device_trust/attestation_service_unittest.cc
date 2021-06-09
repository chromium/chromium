// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation_service.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr char challenge[] =
    "{"
    "\"challenge\": {"
    " \"data\": "
    "\"ChZFbnRlcnByaXNlS2V5Q2hhbGxlbmdlEiAXyp394cl5TtKo+yhlQPa+CQMPbFyIoY//"
    "CXHxvDqVqhiBktSOgi8=\","
    "  \"signature\": "
    "\"BRRaR9cKJe6NRBAUtPLjRujd0BawaYaPpHXzaWxSqCbFZcB2ZnDwFskh4qPeO0EganhwrPBj"
    "bD1yLXY1uiPM38TjYQgj2LFXyq0RjGehZ7qLDv2zebiIn6TIDvRi4rhEoXDg3bpKczBTDgp9im"
    "BQ6QjJS7Pbj0kxPwHzkoVq5UnF9mUUecOAHgKV6ONs4rVjNSpZAPSD/"
    "jC39wDlIXR5YDKSPCs46u66koDyjM7DNVig+S8nTdr14sXEGFSiHyeFaZC5kXQo103bB9j+"
    "tcSpwa0MfuZJ5QFJlB1HipjpaGSImZbJPfkjtoK3F1rn9AiHz+nIjLWPrg3KnQt2eaTNSw==\""
    "}"
    "}";

}  // namespace

namespace enterprise_connectors {

class AttestationServiceTest : public testing::Test {
 public:
  AttestationServiceTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  void SetUp() override {
    testing::Test::SetUp();
    OSCryptMocker::SetUp();
  }

  void TearDown() override {
    testing::Test::TearDown();
    OSCryptMocker::TearDown();
  }

 private:
  ScopedTestingLocalState local_state_;
  base::test::TaskEnvironment task_environment_;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(AttestationServiceTest, BuildChallengeResponse) {
  AttestationService attestation_service_;
  SignEnterpriseChallengeRequest request;
  SignEnterpriseChallengeReply result;
  // Get the challenge from the SignedData json and create request.
  request.set_challenge(
      attestation_service_.JsonChallengeToProtobufChallenge(challenge));
  // If challenge is equal to empty string, then
  // `JsonChallengeToProtobufChallenge()` failed.
  EXPECT_NE(request.challenge(), std::string());

  attestation_service_.SignEnterpriseChallenge(request, &result);
  // If challenge is equal to empty string, then
  // `JsonChallengeToProtobufChallenge()` failed.
  EXPECT_NE(result.challenge_response(), std::string());

  absl::optional<base::Value> challenge_response = base::JSONReader::Read(
      attestation_service_.ProtobufChallengeToJsonChallenge(
          result.challenge_response()),
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  EXPECT_NE(challenge_response.value()
                .FindPath("challengeResponse.data")
                ->GetString(),
            std::string());
  EXPECT_NE(challenge_response.value()
                .FindPath("challengeResponse.signature")
                ->GetString(),
            std::string());
}

}  // namespace enterprise_connectors
