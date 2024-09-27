// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/device_authenticator_android.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge.h"
#include "components/device_reauth/device_authenticator.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Bucket;
using base::test::RunOnceCallback;
using device_reauth::BiometricsAvailability;
using device_reauth::DeviceAuthenticator;
using device_reauth::DeviceAuthUIResult;
using testing::_;
using testing::ElementsAre;
using testing::Return;

class MockDeviceAuthenticatorBridge : public DeviceAuthenticatorBridge {
 public:
  MOCK_METHOD(BiometricsAvailability,
              CanAuthenticateWithBiometric,
              (),
              (override));
  MOCK_METHOD(bool, CanAuthenticateWithBiometricOrScreenLock, (), (override));
  MOCK_METHOD(void,
              Authenticate,
              (base::OnceCallback<void(device_reauth::DeviceAuthUIResult)>
                   response_callback),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
};

}  // namespace

class DeviceAuthenticatorAndroidTest : public testing::Test {
 public:
  DeviceAuthenticatorAndroidTest()
      : device_authenticator_params_(
            base::Seconds(60),
            device_reauth::DeviceAuthSource::kPasswordManager) {}

  void SetUp() override {
    std::unique_ptr<MockDeviceAuthenticatorBridge> bridge =
        std::make_unique<MockDeviceAuthenticatorBridge>();
    bridge_ = bridge.get();
    authenticator_ = std::make_unique<DeviceAuthenticatorAndroid>(
        std::move(bridge), &proxy_, device_authenticator_params_);
  }

  DeviceAuthenticatorAndroid* authenticator() { return authenticator_.get(); }

  MockDeviceAuthenticatorBridge& bridge() { return *bridge_; }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  DeviceAuthenticatorProxy proxy_;
  device_reauth::DeviceAuthParams device_authenticator_params_;
  std::unique_ptr<DeviceAuthenticatorAndroid> authenticator_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // This is owned by the authenticator.
  raw_ptr<MockDeviceAuthenticatorBridge> bridge_ = nullptr;
};

TEST_F(DeviceAuthenticatorAndroidTest, CanAuthenticateCallsBridge) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_TRUE(authenticator()->CanAuthenticateWithBiometrics());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAuthPwdFill.CanAuthenticate",
      BiometricsAvailability::kAvailable, 1);
}

TEST_F(
    DeviceAuthenticatorAndroidTest,
    CanAuthenticateDoesNotReecordHistogramForNonPasswordManagerForIncognito) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_TRUE(authenticator()->CanAuthenticateWithBiometricOrScreenLock());

  histogram_tester.ExpectTotalCount(
      "PasswordManager.BiometricAuthPwdFill.CanAuthenticate", 0);
}

TEST_F(DeviceAuthenticatorAndroidTest, AuthenticateRecordsSource) {
  base::HistogramTester histogram_tester;

  authenticator()->AuthenticateWithMessage(u"", base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Android.DeviceAuthenticator.AuthSource",
      device_reauth::DeviceAuthSource::kPasswordManager, 1);
}

TEST_F(DeviceAuthenticatorAndroidTest, DoesntTriggerAuthIfWithin60Seconds) {
  // Simulate a previous successful authentication
  base::HistogramTester histogram_tester;
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(RunOnceCallback<0>(DeviceAuthUIResult::kSuccessWithBiometrics));
  authenticator()->AuthenticateWithMessage(u"", base::DoNothing());

  // The next call to `Authenticate()` should not re-trigger an authentication.
  EXPECT_CALL(bridge(), Authenticate(_)).Times(0);
  base::MockCallback<DeviceAuthenticator::AuthenticateCallback> result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(u"", result_callback.Get());
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(
          Bucket(
              static_cast<int>(DeviceAuthFinalResult::kSuccessWithBiometrics),
              1),
          Bucket(static_cast<int>(DeviceAuthFinalResult::kAuthStillValid), 1)));
}

TEST_F(DeviceAuthenticatorAndroidTest, TriggersAuthIfMoreThan60Seconds) {
  base::HistogramTester histogram_tester;
  // Simulate a previous successful authentication
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(RunOnceCallback<0>(DeviceAuthUIResult::kSuccessWithBiometrics));
  authenticator()->AuthenticateWithMessage(u"", base::DoNothing());

  task_environment().FastForwardBy(base::Seconds(60));

  // The next call to `Authenticate()` should re-trigger an authentication.
  EXPECT_CALL(bridge(), Authenticate(_))
      .WillOnce(RunOnceCallback<0>(DeviceAuthUIResult::kFailed));
  base::MockCallback<DeviceAuthenticator::AuthenticateCallback> result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  authenticator()->AuthenticateWithMessage(u"", result_callback.Get());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(Bucket(static_cast<int>(
                             DeviceAuthFinalResult::kSuccessWithBiometrics),
                         1),
                  Bucket(static_cast<int>(DeviceAuthFinalResult::kFailed), 1)));
}

TEST_F(DeviceAuthenticatorAndroidTest, TriggersAuthIfPreviousFailed) {
  base::HistogramTester histogram_tester;
  // Simulate a previous failed authentication
  EXPECT_CALL(bridge(), Authenticate)
      .WillOnce(RunOnceCallback<0>(DeviceAuthUIResult::kFailed));
  authenticator()->AuthenticateWithMessage(u"", base::DoNothing());

  // The next call to `Authenticate()` should re-trigger an authentication.
  EXPECT_CALL(bridge(), Authenticate(_))
      .WillOnce(RunOnceCallback<0>(DeviceAuthUIResult::kSuccessWithBiometrics));
  base::MockCallback<DeviceAuthenticator::AuthenticateCallback> result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(u"", result_callback.Get());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.BiometricAuthPwdFill.AuthResult"),
      ElementsAre(Bucket(static_cast<int>(
                             DeviceAuthFinalResult::kSuccessWithBiometrics),
                         1),
                  Bucket(static_cast<int>(DeviceAuthFinalResult::kFailed), 1)));
}

TEST_F(DeviceAuthenticatorAndroidTest, GetBiometricAvailabilityStatusRequired) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kRequired));
  EXPECT_EQ(device_reauth::BiometricStatus::kRequired,
            authenticator()->GetBiometricAvailabilityStatus());
}

TEST_F(DeviceAuthenticatorAndroidTest,
       GetBiometricAvailabilityStatusRequiredButHasErrors) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kRequiredButHasError));
  EXPECT_EQ(device_reauth::BiometricStatus::kRequired,
            authenticator()->GetBiometricAvailabilityStatus());
}

TEST_F(DeviceAuthenticatorAndroidTest,
       GetBiometricAvailabilityStatusAvailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kAvailable));
  EXPECT_EQ(device_reauth::BiometricStatus::kBiometricsAvailable,
            authenticator()->GetBiometricAvailabilityStatus());
}

TEST_F(DeviceAuthenticatorAndroidTest,
       GetBiometricAvailabilityStatusLSKFAvailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kNotEnrolled));
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_EQ(device_reauth::BiometricStatus::kOnlyLskfAvailable,
            authenticator()->GetBiometricAvailabilityStatus());
}

TEST_F(DeviceAuthenticatorAndroidTest,
       GetBiometricAvailabilityStatusUnavailable) {
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometric)
      .WillOnce(Return(BiometricsAvailability::kNotEnrolled));
  EXPECT_CALL(bridge(), CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(false));
  EXPECT_EQ(device_reauth::BiometricStatus::kUnavailable,
            authenticator()->GetBiometricAvailabilityStatus());
}
