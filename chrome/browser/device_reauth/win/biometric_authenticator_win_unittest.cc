// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/biometric_authenticator_win.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using device_reauth::BiometricAuthenticator;
using device_reauth::BiometricAuthRequester;
using password_manager::PasswordAccessAuthenticator;
using testing::_;
using testing::Return;

class MockSystemAuthenticator : public AuthenticatorWinInterface {
 public:
  MOCK_METHOD(void,
              AuthenticateUser,
              (const std::u16string& message,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              CheckIfBiometricsAvailable,
              (AvailabilityCallback callback),
              (override));
};

class BiometricAuthenticatorWinTest : public testing::Test {
 public:
  BiometricAuthenticatorWinTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  void SetUp() override {
    std::unique_ptr<MockSystemAuthenticator> system_authenticator =
        std::make_unique<MockSystemAuthenticator>();
    system_authenticator_ = system_authenticator.get();
    authenticator_ = BiometricAuthenticatorWin::CreateForTesting(
        std::move(system_authenticator));
  }

  BiometricAuthenticatorWin* authenticator() { return authenticator_.get(); }

  MockSystemAuthenticator& system_authenticator() {
    return *system_authenticator_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  ScopedTestingLocalState& local_state() { return testing_local_state_; }

  void ExpectAuthenticationAndSetResult(bool result) {
    EXPECT_CALL(system_authenticator(), AuthenticateUser)
        .WillOnce(testing::WithArg<1>([result](auto callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback), /*auth_succeeded=*/result));
        }));
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<BiometricAuthenticatorWin> authenticator_;

  ScopedTestingLocalState testing_local_state_;

  // This is owned by the authenticator.
  raw_ptr<MockSystemAuthenticator> system_authenticator_ = nullptr;
};

// If time that passed since the last successful authentication is smaller than
// kAuthValidityPeriod, no reauthentication is needed.
TEST_F(BiometricAuthenticatorWinTest,
       NoReauthenticationIfLessThanAuthValidityPeriod) {
  ExpectAuthenticationAndSetResult(true);
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  // The delay is smaller than kAuthValidityPeriod there shouldn't be
  // another prompt, so the auth should be reported as successful.
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);

  EXPECT_CALL(system_authenticator(), AuthenticateUser).Times(0);
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback.Get());

  task_environment().RunUntilIdle();
}

// If the time since the last reauthentication is greater than
// kAuthValidityPeriod reauthentication is needed.
TEST_F(BiometricAuthenticatorWinTest, ReauthenticationIfMoreThan60Seconds) {
  // Simulate a previous successful authentication
  ExpectAuthenticationAndSetResult(true);
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod * 2);

  // The next call to `Authenticate()` should re-trigger an authentication.
  ExpectAuthenticationAndSetResult(false);
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/false));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback.Get());

  task_environment().RunUntilIdle();
}

// If previous authentication failed, kAuthValidityPeriod isn't started and
// reauthentication will be needed.
TEST_F(BiometricAuthenticatorWinTest, ReauthenticationIfPreviousFailed) {
  ExpectAuthenticationAndSetResult(false);
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());
  task_environment().RunUntilIdle();

  // The next call to `Authenticate()` should re-trigger an authentication.
  ExpectAuthenticationAndSetResult(true);
  base::MockCallback<BiometricAuthenticator::AuthenticateCallback>
      result_callback;
  EXPECT_CALL(result_callback, Run(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback.Get());

  task_environment().RunUntilIdle();
}

// Checks if CanAuthenticate returns a valid pref value.
TEST_F(BiometricAuthenticatorWinTest, CanAuthenticate) {
  local_state().Get()->SetBoolean(
      password_manager::prefs::kIsBiometricAvailable, true);
  EXPECT_TRUE(authenticator()->CanAuthenticate(
      BiometricAuthRequester::kPasswordsInSettings));

  local_state().Get()->SetBoolean(
      password_manager::prefs::kIsBiometricAvailable, false);
  EXPECT_FALSE(authenticator()->CanAuthenticate(
      BiometricAuthRequester::kPasswordsInSettings));
}

// Verifies that the caching mechanism for BiometricsAvailable works.
struct TestCase {
  const char* description;
  BiometricAuthenticationStatusWin availability;
  bool expected_result;
  int expected_bucket;
};

class BiometricAuthenticatorWinTestAvailability
    : public BiometricAuthenticatorWinTest,
      public testing::WithParamInterface<TestCase> {};

TEST_P(BiometricAuthenticatorWinTestAvailability, AvailabilityCheck) {
  base::HistogramTester histogram_tester;
  TestCase test_case = GetParam();
  SCOPED_TRACE(test_case.description);
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(testing::WithArg<0>([&test_case](auto callback) {
        std::move(callback).Run(test_case.availability);
      }));
  authenticator()->CacheIfBiometricsAvailable();

  EXPECT_EQ(test_case.expected_result,
            authenticator()->CanAuthenticate(
                BiometricAuthRequester::kPasswordsInSettings));
  EXPECT_EQ(test_case.expected_result,
            local_state().Get()->GetBoolean(
                password_manager::prefs::kHadBiometricsAvailable));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAvailabilityWin", test_case.expected_bucket, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BiometricAuthenticatorWinTestAvailability,
    ::testing::Values(
        TestCase{
            .description = "kUnknown",
            .availability = BiometricAuthenticationStatusWin::kUnknown,
            .expected_result = false,
            .expected_bucket = 0,
        },
        TestCase{
            .description = "kAvailable",
            .availability = BiometricAuthenticationStatusWin::kAvailable,
            .expected_result = true,
            .expected_bucket = 1,
        },
        TestCase{
            .description = "kDeviceBusy",
            .availability = BiometricAuthenticationStatusWin::kDeviceBusy,
            .expected_result = false,
            .expected_bucket = 2,
        },
        TestCase{
            .description = "kDisabledByPolicy",
            .availability = BiometricAuthenticationStatusWin::kDisabledByPolicy,
            .expected_result = false,
            .expected_bucket = 3,
        },
        TestCase{
            .description = "kDeviceNotPresent",
            .availability = BiometricAuthenticationStatusWin::kDeviceNotPresent,
            .expected_result = false,
            .expected_bucket = 4,
        },
        TestCase{
            .description = "kNotConfiguredForUser",
            .availability =
                BiometricAuthenticationStatusWin::kNotConfiguredForUser,
            .expected_result = false,
            .expected_bucket = 5,
        }));

}  // namespace
