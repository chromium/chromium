// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key_subtle.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::StrictMock;

class Profile;

namespace ash {
namespace attestation {
namespace {

constexpr char kEmptyKeyName[] = "";
constexpr char kNonDefaultKeyName[] = "key_name_123";

std::string GetChallenge() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'c',  'h',
                                 'a', 'l', 0xfd, 0xfe, 0xff};
  return std::string(reinterpret_cast<const char*>(kBuffer), sizeof(kBuffer));
}

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

class TpmChallengeKeyTest : public ::testing::Test {
 public:
  TpmChallengeKeyTest() {
    auto mock_challenge_key_subtle =
        std::make_unique<StrictMock<MockTpmChallengeKeySubtle>>();
    mock_tpm_challenge_key_subtle_ = mock_challenge_key_subtle.get();
    TpmChallengeKeySubtleFactory::SetForTesting(
        std::move(mock_challenge_key_subtle));

    challenge_key_ = TpmChallengeKeyFactory::Create();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<MockTpmChallengeKeySubtle, DanglingUntriaged>
      mock_tpm_challenge_key_subtle_ = nullptr;
  std::unique_ptr<TpmChallengeKey> challenge_key_;
  // In the current implementation of TpmChallengeKey the profile is just
  // forwarded, so actual value does not matter.
  TestingProfile profile_;
};

class CallbackObserver {
 public:
  TpmChallengeKeyCallback GetCallback() {
    return base::BindOnce(&CallbackObserver::Callback, base::Unretained(this));
  }

  const TpmChallengeKeyResult& GetResult() const {
    CHECK(result_.has_value()) << "Callback was never called";
    return result_.value();
  }

  void WaitForCallback() { loop_.Run(); }

 private:
  void Callback(const TpmChallengeKeyResult& result) {
    CHECK(!result_.has_value()) << "Callback was called more than once";
    result_ = result;
    loop_.Quit();
  }

  base::RunLoop loop_;
  std::optional<TpmChallengeKeyResult> result_;
};

TEST_F(TpmChallengeKeyTest, PrepareKeyFailed) {
  const ::attestation::VerifiedAccessFlow kFlowType =
      ::attestation::ENTERPRISE_MACHINE;
  const bool kRegisterKey = false;
  const char* const kKeyName = kEmptyKeyName;

  EXPECT_CALL(
      *mock_tpm_challenge_key_subtle_,
      StartPrepareKeyStep(kFlowType, kRegisterKey, ::attestation::KEY_TYPE_RSA,
                          kKeyName, &profile_,
                          /*callback=*/_, /*signals=*/_))
      .WillOnce(RunOnceCallback<5>(TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kGetCertificateFailedError)));

  CallbackObserver callback_observer;
  challenge_key_->BuildResponse(kFlowType, &profile_,
                                callback_observer.GetCallback(), GetChallenge(),
                                kRegisterKey, ::attestation::KEY_TYPE_RSA,
                                kKeyName, /*signals=*/std::nullopt);
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(),
            TpmChallengeKeyResult::MakeError(
                TpmChallengeKeyResultCode::kGetCertificateFailedError));
}

TEST_F(TpmChallengeKeyTest, SignChallengeFailed) {
  const ::attestation::VerifiedAccessFlow kFlowType =
      ::attestation::ENTERPRISE_USER;
  const bool kRegisterKey = true;
  const char* const kKeyName = kNonDefaultKeyName;

  EXPECT_CALL(
      *mock_tpm_challenge_key_subtle_,
      StartPrepareKeyStep(kFlowType, kRegisterKey, ::attestation::KEY_TYPE_RSA,
                          kKeyName, &profile_,
                          /*callback=*/_, /*signals=*/_))
      .WillOnce(RunOnceCallback<5>(
          TpmChallengeKeyResult::MakePublicKey(GetPublicKey())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartSignChallengeStep(GetChallenge(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kSignChallengeFailedError)));

  CallbackObserver callback_observer;
  challenge_key_->BuildResponse(kFlowType, &profile_,
                                callback_observer.GetCallback(), GetChallenge(),
                                kRegisterKey, ::attestation::KEY_TYPE_RSA,
                                kKeyName, /*signals=*/std::nullopt);
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(),
            TpmChallengeKeyResult::MakeError(
                TpmChallengeKeyResultCode::kSignChallengeFailedError));
}

TEST_F(TpmChallengeKeyTest, RegisterKeyFailed) {
  const ::attestation::VerifiedAccessFlow kFlowType =
      ::attestation::ENTERPRISE_USER;
  const bool kRegisterKey = true;
  const char* const kKeyName = kNonDefaultKeyName;

  EXPECT_CALL(
      *mock_tpm_challenge_key_subtle_,
      StartPrepareKeyStep(kFlowType, kRegisterKey, ::attestation::KEY_TYPE_RSA,
                          kKeyName, &profile_,
                          /*callback=*/_, /*signals=*/_))
      .WillOnce(RunOnceCallback<5>(
          TpmChallengeKeyResult::MakePublicKey(GetPublicKey())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartSignChallengeStep(GetChallenge(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(TpmChallengeKeyResult::MakeChallengeResponse(
          GetChallengeResponse())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartRegisterKeyStep(/*callback=*/_))
      .WillOnce(RunOnceCallback<0>(TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kKeyRegistrationFailedError)));

  CallbackObserver callback_observer;
  challenge_key_->BuildResponse(kFlowType, &profile_,
                                callback_observer.GetCallback(), GetChallenge(),
                                kRegisterKey, ::attestation::KEY_TYPE_RSA,
                                kKeyName, /*signals=*/std::nullopt);
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(),
            TpmChallengeKeyResult::MakeError(
                TpmChallengeKeyResultCode::kKeyRegistrationFailedError));
}

TEST_F(TpmChallengeKeyTest, DontRegisterSuccess) {
  const ::attestation::VerifiedAccessFlow kFlowType =
      ::attestation::ENTERPRISE_USER;
  const bool kRegisterKey = false;
  const char* const kKeyName = kEmptyKeyName;

  EXPECT_CALL(
      *mock_tpm_challenge_key_subtle_,
      StartPrepareKeyStep(kFlowType, kRegisterKey, ::attestation::KEY_TYPE_RSA,
                          kKeyName, &profile_,
                          /*callback=*/_, /*signals=*/_))
      .WillOnce(RunOnceCallback<5>(
          TpmChallengeKeyResult::MakePublicKey(GetPublicKey())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartSignChallengeStep(GetChallenge(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(TpmChallengeKeyResult::MakeChallengeResponse(
          GetChallengeResponse())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartRegisterKeyStep(/*callback=*/_))
      .Times(0);

  CallbackObserver callback_observer;
  challenge_key_->BuildResponse(kFlowType, &profile_,
                                callback_observer.GetCallback(), GetChallenge(),
                                kRegisterKey, ::attestation::KEY_TYPE_RSA,
                                kKeyName, /*signals=*/std::nullopt);
  callback_observer.WaitForCallback();

  EXPECT_EQ(
      callback_observer.GetResult(),
      TpmChallengeKeyResult::MakeChallengeResponse(GetChallengeResponse()));
}

TEST_F(TpmChallengeKeyTest, RegisterSuccess) {
  const ::attestation::VerifiedAccessFlow kFlowType =
      ::attestation::ENTERPRISE_USER;
  const bool kRegisterKey = true;
  const char* const kKeyName = kEmptyKeyName;

  EXPECT_CALL(
      *mock_tpm_challenge_key_subtle_,
      StartPrepareKeyStep(kFlowType, kRegisterKey, ::attestation::KEY_TYPE_RSA,
                          kKeyName, &profile_,
                          /*callback=*/_, /*signals=*/_))
      .WillOnce(RunOnceCallback<5>(
          TpmChallengeKeyResult::MakePublicKey(GetPublicKey())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartSignChallengeStep(GetChallenge(), /*callback=*/_))
      .WillOnce(RunOnceCallback<1>(TpmChallengeKeyResult::MakeChallengeResponse(
          GetChallengeResponse())));

  EXPECT_CALL(*mock_tpm_challenge_key_subtle_,
              StartRegisterKeyStep(/*callback=*/_))
      .WillOnce(RunOnceCallback<0>(TpmChallengeKeyResult::MakeSuccess()));

  CallbackObserver callback_observer;
  challenge_key_->BuildResponse(kFlowType, &profile_,
                                callback_observer.GetCallback(), GetChallenge(),
                                kRegisterKey, ::attestation::KEY_TYPE_RSA,
                                kKeyName, /*signals=*/std::nullopt);
  callback_observer.WaitForCallback();

  EXPECT_EQ(
      callback_observer.GetResult(),
      TpmChallengeKeyResult::MakeChallengeResponse(GetChallengeResponse()));
}

}  // namespace
}  // namespace attestation
}  // namespace ash
