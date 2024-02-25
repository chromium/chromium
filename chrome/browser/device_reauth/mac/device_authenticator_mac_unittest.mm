// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/device_authenticator_mac.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/device_reauth/device_reauth_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#include "device/fido/mac/touch_id_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using MockAuthResultCallback =
    base::MockCallback<DeviceAuthenticatorMac::AuthenticateCallback>;
using device_reauth::ReauthResult;

constexpr base::TimeDelta kAuthValidityPeriod = base::Seconds(60);
constexpr char kHistogramName[] =
    "PasswordManager.ReauthToAccessPasswordInSettings";

}  // namespace

class MockSystemAuthenticator : public AuthenticatorMacInterface {
 public:
  MOCK_METHOD(bool, CheckIfBiometricsAvailable, (), (override));
  MOCK_METHOD(bool, CheckIfBiometricsOrScreenLockAvailable, (), (override));
  MOCK_METHOD(bool,
              AuthenticateUserWithNonBiometrics,
              (const std::u16string&),
              (override));
};

// Test params decides whether biometric authentication and screen lock are
// available.
class DeviceAuthenticatorMacTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  DeviceAuthenticatorMacTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()),
        device_authenticator_params_(
            kAuthValidityPeriod,
            device_reauth::DeviceAuthSource::kPasswordManager,
            kHistogramName) {
    std::unique_ptr<MockSystemAuthenticator> system_authenticator =
        std::make_unique<MockSystemAuthenticator>();
    system_authenticator_ = system_authenticator.get();
    authenticator_ = std::make_unique<DeviceAuthenticatorMac>(
        std::move(system_authenticator), &proxy_, device_authenticator_params_);
    ON_CALL(*system_authenticator_, CheckIfBiometricsAvailable)
        .WillByDefault(testing::Return(is_biometric_available()));
    ON_CALL(*system_authenticator_, CheckIfBiometricsOrScreenLockAvailable)
        .WillByDefault(testing::Return(is_biometric_available() ||
                                       is_screen_lock_available()));
  }

  bool is_biometric_available() { return std::get<0>(GetParam()); }
  bool is_screen_lock_available() { return std::get<1>(GetParam()); }

  void SimulateReauthSuccess() {
    if (is_biometric_available()) {
      touch_id_environment()->SimulateTouchIdPromptSuccess();
    } else {
      EXPECT_CALL(system_authenticator(), AuthenticateUserWithNonBiometrics)
          .WillOnce(testing::Return(true));
    }
  }

  void SimulateReauthFailure() {
    if (is_biometric_available()) {
      touch_id_environment()->SimulateTouchIdPromptFailure();
    } else {
      EXPECT_CALL(system_authenticator(), AuthenticateUserWithNonBiometrics)
          .WillOnce(testing::Return(false));
    }
  }

  device_reauth::DeviceAuthenticator* authenticator() {
    return authenticator_.get();
  }

  MockSystemAuthenticator& system_authenticator() {
    return *system_authenticator_;
  }

  ScopedTestingLocalState& local_state() { return testing_local_state_; }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  device::fido::mac::ScopedTouchIdTestEnvironment* touch_id_environment() {
    return &touch_id_test_environment_;
  }

  MockAuthResultCallback& result_callback() { return result_callback_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  DeviceAuthenticatorProxy proxy_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState testing_local_state_;
  device_reauth::DeviceAuthParams device_authenticator_params_;
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;
  device::fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment_{
      config_};
  MockAuthResultCallback result_callback_;
  base::HistogramTester histogram_tester_;

  // This is owned by the authenticator.
  raw_ptr<MockSystemAuthenticator> system_authenticator_ = nullptr;
};

// If time that passed since the last successful authentication is smaller than
// kAuthValidityPeriod, no reauthentication is needed.
TEST_P(DeviceAuthenticatorMacTest, NoReauthenticationIfLessThan60Seconds) {
  SimulateReauthSuccess();
  EXPECT_CALL(result_callback(), Run(/*success=*/true));

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Since the delay is smaller than kAuthValidityPeriod there shouldn't be
  // another prompt, so the auth should be reported as successful. If there is a
  // call to touchIdContext test will fail as TouchIdEnvironment will crash
  // since there is no prompt expected.
  task_environment().FastForwardBy(kAuthValidityPeriod / 2);

  EXPECT_CALL(result_callback(), Run(/*success=*/true));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());
}

// If the time since the last reauthentication is greater than
// kAuthValidityPeriod or the authentication failed, reauthentication is needed.
TEST_P(DeviceAuthenticatorMacTest, ReauthenticationIfMoreThan60Seconds) {
  SimulateReauthSuccess();
  EXPECT_CALL(result_callback(), Run(/*success=*/true));

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Make the reauth prompt auth fail.
  SimulateReauthFailure();
  // Since the delay is bigger than kAuthValidityPeriod, the previous auth has
  // expired. Thus a new prompt will be requested which should fail the
  // authentication.
  task_environment().FastForwardBy(kAuthValidityPeriod * 2);

  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());
}

// If previous authentication failed kAuthValidityPeriod isn't started and
// reauthentication will be needed.
TEST_P(DeviceAuthenticatorMacTest, ReauthenticationIfPreviousFailed) {
  SimulateReauthFailure();

  // First authentication fails, no last_good_auth_timestamp_ should be
  // recorded, which fill force reauthentication.
  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Although it passed less than kAuthValidityPeriod no valid authentication
  // should be recorded as reauth will fail.
  SimulateReauthFailure();
  task_environment().FastForwardBy(kAuthValidityPeriod / 2);

  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());
}

// If pending authentication can be canceled.
TEST_P(DeviceAuthenticatorMacTest, CancelPendingAuthentication) {
  // Non-biometric reauth is modal, and hence cannot be requested twice.
  if (!is_biometric_available()) {
    return;
  }
  touch_id_environment()->SimulateTouchIdPromptSuccess();
  touch_id_environment()->DoNotResolveNextPrompt();

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Authentication should fail as it will take 10 seconds to authenticate, and
  // there will be a cancellation in the meantime.
  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->Cancel();
}

TEST_P(DeviceAuthenticatorMacTest, BiometricAuthenticationAvailability) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable);
  EXPECT_EQ(authenticator()->CanAuthenticateWithBiometrics(),
            is_biometric_available());
  EXPECT_EQ(is_biometric_available(),
            local_state().Get()->GetBoolean(
                password_manager::prefs::kHadBiometricsAvailable));
}

TEST_P(DeviceAuthenticatorMacTest,
       BiometricAndScreenLockAuthenticationAvailablity) {
  if (is_biometric_available()) {
    EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable);
  } else {
    EXPECT_CALL(system_authenticator(), CheckIfBiometricsOrScreenLockAvailable);
  }

  EXPECT_EQ(authenticator()->CanAuthenticateWithBiometricOrScreenLock(),
            is_biometric_available() || is_screen_lock_available());
  EXPECT_EQ(is_biometric_available(),
            local_state().Get()->GetBoolean(
                password_manager::prefs::kHadBiometricsAvailable));
}

TEST_P(DeviceAuthenticatorMacTest, RecordSuccessAuthHistogram) {
  SimulateReauthSuccess();

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  histogram_tester().ExpectUniqueSample(kHistogramName, ReauthResult::kSuccess,
                                        1);
}

TEST_P(DeviceAuthenticatorMacTest, RecordSkippedAuthHistogram) {
  SimulateReauthSuccess();

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSuccess,
                                       1);
  histogram_tester().ExpectBucketCount(kHistogramName, ReauthResult::kSkipped,
                                       1);
}

TEST_P(DeviceAuthenticatorMacTest, RecordFailAuthHistogram) {
  SimulateReauthFailure();

  authenticator()->AuthenticateWithMessage(
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  histogram_tester().ExpectUniqueSample(kHistogramName, ReauthResult::kFailure,
                                        1);
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceAuthenticatorMacTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
