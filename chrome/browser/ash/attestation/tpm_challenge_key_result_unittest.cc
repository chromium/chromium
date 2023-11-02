// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {
namespace {

std::string GetChallengeResponse() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'r',  'e',
                                 's', 'p', 0xfd, 0xfe, 0xff};
  return std::string(reinterpret_cast<const char*>(kBuffer), sizeof(kBuffer));
}

std::string GetPublicKey() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'p',  'u',
                                 'b', 'k', 0xfd, 0xfe, 0xff};
  return std::string(reinterpret_cast<const char*>(kBuffer), sizeof(kBuffer));
}

std::string GetPublicKey2() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'p',  'u', 'b',
                                 'k', '2', 0xfd, 0xfe, 0xff};
  return std::string(reinterpret_cast<const char*>(kBuffer), sizeof(kBuffer));
}

TEST(TpmChallengeKeyResultTest, MakeChallengeResponse) {
  TpmChallengeKeyResult result =
      TpmChallengeKeyResult::MakeChallengeResponse(GetChallengeResponse());

  EXPECT_TRUE(result.IsSuccess());
  EXPECT_EQ(result.result_code, TpmChallengeKeyResultCode::kSuccess);
  EXPECT_EQ(result.challenge_response, GetChallengeResponse());
  EXPECT_EQ(result.public_key, "");
}

TEST(TpmChallengeKeyResultTest, MakePublicKey) {
  TpmChallengeKeyResult result =
      TpmChallengeKeyResult::MakePublicKey(GetPublicKey());

  EXPECT_TRUE(result.IsSuccess());
  EXPECT_EQ(result.result_code, TpmChallengeKeyResultCode::kSuccess);
  EXPECT_EQ(result.challenge_response, "");
  EXPECT_EQ(result.public_key, GetPublicKey());
}

TEST(TpmChallengeKeyResultTest, MakeSuccess) {
  TpmChallengeKeyResult result = TpmChallengeKeyResult::MakeSuccess();

  EXPECT_TRUE(result.IsSuccess());
  EXPECT_EQ(result.result_code, TpmChallengeKeyResultCode::kSuccess);
  EXPECT_EQ(result.challenge_response, "");
  EXPECT_EQ(result.public_key, "");
}

TEST(TpmChallengeKeyResultTest, MakeError) {
  TpmChallengeKeyResult result = TpmChallengeKeyResult::MakeError(
      TpmChallengeKeyResultCode::kGetPublicKeyFailedError);

  EXPECT_FALSE(result.IsSuccess());
  EXPECT_EQ(result.result_code,
            TpmChallengeKeyResultCode::kGetPublicKeyFailedError);
  EXPECT_EQ(result.challenge_response, "");
  EXPECT_EQ(result.public_key, "");
  EXPECT_EQ(result.GetErrorMessage(),
            TpmChallengeKeyResult::kGetPublicKeyFailedErrorMsg);
}

TEST(TpmChallengeKeyResultTest, OperatorEqual) {
  TpmChallengeKeyResult result1 =
      TpmChallengeKeyResult::MakeError(TpmChallengeKeyResultCode::kDbusError);
  TpmChallengeKeyResult result2 =
      TpmChallengeKeyResult::MakePublicKey(GetPublicKey());
  TpmChallengeKeyResult result3 =
      TpmChallengeKeyResult::MakePublicKey(GetPublicKey2());

  EXPECT_TRUE(result1 == result1);
  EXPECT_EQ(result1, TpmChallengeKeyResult::MakeError(
                         TpmChallengeKeyResultCode::kDbusError));
  EXPECT_TRUE(result1 != result2);
  EXPECT_TRUE(result2 != result3);
}

}  // namespace
}  // namespace attestation
}  // namespace ash
