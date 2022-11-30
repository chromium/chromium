// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/biometric_authenticator_win.h"

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
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
  MOCK_METHOD(bool,
              AuthenticateUser,
              (const std::u16string& message),
              (override));
  MOCK_METHOD(void,
              CheckIfBiometricsAvailable,
              (AvailabilityCallback callback),
              (override));
};

}  // namespace

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

  void SetBiometricAvailability(bool available) {
    testing_local_state_.Get()->SetBoolean(
        password_manager::prefs::kIsBiometricAvailable, available);
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
  EXPECT_CALL(system_authenticator(), AuthenticateUser)
      .WillOnce(Return(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  // The delay is smaller than kAuthValidityPeriod there shouldn't be
  // another prompt, so the auth should be reported as successful.
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);

  EXPECT_CALL(system_authenticator(), AuthenticateUser(_)).Times(0);
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
  EXPECT_CALL(system_authenticator(), AuthenticateUser)
      .WillOnce(Return(/*auth_succeeded=*/true));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod * 2);

  // The next call to `Authenticate()` should re-trigger an authentication.
  EXPECT_CALL(system_authenticator(), AuthenticateUser(_))
      .WillOnce(Return(/*auth_succeeded=*/false));
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
  EXPECT_CALL(system_authenticator(), AuthenticateUser)
      .WillOnce(Return(/*auth_succeeded=*/false));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.", base::DoNothing());

  // The next call to `Authenticate()` should re-trigger an authentication.
  EXPECT_CALL(system_authenticator(), AuthenticateUser(_))
      .WillOnce(Return(true));
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
  SetBiometricAvailability(true);
  EXPECT_TRUE(authenticator()->CanAuthenticate(
      BiometricAuthRequester::kPasswordsInSettings));

  SetBiometricAvailability(false);
  EXPECT_FALSE(authenticator()->CanAuthenticate(
      BiometricAuthRequester::kPasswordsInSettings));
}

// Verifies that the caching mechanism for BiometricsAvailable works.
TEST_F(BiometricAuthenticatorWinTest, SavingBiometricsAvailability) {
  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(testing::WithArg<0>(
          [](auto callback) { std::move(callback).Run(true); }));
  authenticator()->CacheIfBiometricsAvailable();
  EXPECT_TRUE(authenticator()->CanAuthenticate(
      BiometricAuthRequester::kPasswordsInSettings));

  EXPECT_CALL(system_authenticator(), CheckIfBiometricsAvailable)
      .WillOnce(testing::WithArg<0>(
          [](auto callback) { std::move(callback).Run(false); }));
  authenticator()->CacheIfBiometricsAvailable();
  EXPECT_FALSE(authenticator()->CanAuthenticate(
      BiometricAuthRequester::kPasswordsInSettings));
}
