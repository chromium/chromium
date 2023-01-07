// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Bucket;
using base::test::RunOnceCallback;
using device_reauth::BiometricAuthenticator;
using device_reauth::BiometricAuthRequester;
using device_reauth::BiometricAuthUIResult;
using device_reauth::BiometricsAvailability;
using testing::_;
using testing::ElementsAre;
using testing::Return;

class MockBiometricAuthenticatorBridge : public BiometricAuthenticatorBridge {
 public:
  MOCK_METHOD(BiometricsAvailability,
              CanAuthenticateWithBiometric,
              (),
              (override));
  MOCK_METHOD(bool, CanAuthenticateWithBiometricOrScreenLock, (), (override));
  MOCK_METHOD(void,
              Authenticate,
              (base::OnceCallback<void(device_reauth::BiometricAuthUIResult)>
                   response_callback),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
};

}  // namespace

class BiometricAuthenticatorAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<MockBiometricAuthenticatorBridge> bridge =
        std::make_unique<MockBiometricAuthenticatorBridge>();
    bridge_ = bridge.get();
    authenticator_ =
        BiometricAuthenticatorAndroid::CreateForTesting(std::move(bridge));
  }

  BiometricAuthenticatorAndroid* authenticator() {
    return authenticator_.get();
  }

  MockBiometricAuthenticatorBridge& bridge() { return *bridge_; }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  scoped_refptr<BiometricAuthenticatorAndroid> authenticator_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // This is owned by the authenticator.
  raw_ptr<MockBiometricAuthenticatorBridge> bridge_ = nullptr;
};

TEST_F(BiometricAuthenticatorAndroidTest, CanAuthenticateCallsBridge) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_TRUE(authenticator()->CanAuthenticate(
      device_reauth::BiometricAuthRequester::kAllPasswordsList));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAuthPwdFill.CanAuthenticate",
      BiometricsAvailability::kAvailable, 1);
}

TEST_F(
    BiometricAuthenticatorAndroidTest,
    CanAuthenticateDoesNotReecordHistogramForNonPasswordManagerForIncognito) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_TRUE(authenticator()->CanAuthenticate(
      device_reauth::BiometricAuthRequester::kIncognitoReauthPage));

  histogram_tester.ExpectTotalCount(
      "PasswordManager.BiometricAuthPwdFill.CanAuthenticate", 0);
}

TEST_F(BiometricAuthenticatorAndroidTest, AuthenticateRecordsRequester) {
  base::HistogramTester histogram_tester;

  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);

  histogram_tester.ExpectUniqueSample("Android.BiometricAuth.AuthRequester",
                                      BiometricAuthRequester::kAllPasswordsList,
                                      1);
}

TEST_F(BiometricAuthenticatorAndroidTest, DoesntTriggerAuthIfWithin60Seconds) {
  // Simulate a previous successful authentication
  base::HistogramTester histogram_tester;
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(
          RunOnceCallback<0>(BiometricAuthUIResult::kSuccessWithBiometrics));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);

  // The next call to `Authenticate()` should not re-trigger an authentication.
  EXPECT_CALL(bridge(), Authenticate(_)).Times(0);
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                result_callback.Get(),
                                /*use_last_valid_auth=*/true);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(
          Bucket(static_cast<int>(
                     BiometricAuthFinalResult::kSuccessWithBiometrics),
                 1),
          Bucket(static_cast<int>(BiometricAuthFinalResult::kAuthStillValid),
                 1)));
}

TEST_F(BiometricAuthenticatorAndroidTest, TriggersAuthIfMoreThan60Seconds) {
  base::HistogramTester histogram_tester;
  // Simulate a previous successful authentication
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(
          RunOnceCallback<0>(BiometricAuthUIResult::kSuccessWithBiometrics));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);

  task_environment().FastForwardBy(base::Seconds(60));

  // The next call to `Authenticate()` should re-trigger an authentication.
  EXPECT_CALL(bridge(), Authenticate(_))
      .WillOnce(RunOnceCallback<0>(BiometricAuthUIResult::kFailed));
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                result_callback.Get(),
                                /*use_last_valid_auth=*/true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(
          Bucket(static_cast<int>(
                     BiometricAuthFinalResult::kSuccessWithBiometrics),
                 1),
          Bucket(static_cast<int>(BiometricAuthFinalResult::kFailed), 1)));
}

TEST_F(BiometricAuthenticatorAndroidTest,
       TriggersAuthIfWithin60Seconds_AndUseLastValidAuthIsFalse) {
  base::HistogramTester histogram_tester;
  // Simulate a previous successful authentication
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(
          RunOnceCallback<0>(BiometricAuthUIResult::kSuccessWithBiometrics));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);

  // The next call to `Authenticate()` should re-trigger an authentication
  // as |use_last_valid_auth| is set to false.
  EXPECT_CALL(bridge(), Authenticate(_))
      .WillOnce(RunOnceCallback<0>(BiometricAuthUIResult::kFailed));
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                result_callback.Get(),
                                /*use_last_valid_auth=*/false);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(
          Bucket(static_cast<int>(
                     BiometricAuthFinalResult::kSuccessWithBiometrics),
                 1),
          Bucket(static_cast<int>(BiometricAuthFinalResult::kFailed), 1)));
}

TEST_F(BiometricAuthenticatorAndroidTest, TriggersAuthIfPreviousFailed) {
  base::HistogramTester histogram_tester;
  // Simulate a previous failed authentication
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(RunOnceCallback<0>(BiometricAuthUIResult::kFailed));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);

  // The next call to `Authenticate()` should re-trigger an authentication.
  EXPECT_CALL(bridge(), Authenticate(_))
      .WillOnce(
          RunOnceCallback<0>(BiometricAuthUIResult::kSuccessWithBiometrics));
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                result_callback.Get(),
                                /*use_last_valid_auth=*/true);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(
          Bucket(static_cast<int>(
                     BiometricAuthFinalResult::kSuccessWithBiometrics),
                 1),
          Bucket(static_cast<int>(BiometricAuthFinalResult::kFailed), 1)));
}

TEST_F(BiometricAuthenticatorAndroidTest, CancelsOngoingAuthIfSameRequester) {
  EXPECT_CALL(bridge(), Authenticate);
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);
  EXPECT_CALL(bridge(), Cancel);
  authenticator()->Cancel(BiometricAuthRequester::kAllPasswordsList);
}

TEST_F(BiometricAuthenticatorAndroidTest, DoesntCancelAuthIfNotSameRequester) {
  EXPECT_CALL(bridge(), Authenticate);
  authenticator()->Authenticate(BiometricAuthRequester::kAllPasswordsList,
                                base::DoNothing(),
                                /*use_last_valid_auth=*/true);
  EXPECT_CALL(bridge(), Cancel).Times(0);
  authenticator()->Cancel(BiometricAuthRequester::kAccountChooserDialog);
}
