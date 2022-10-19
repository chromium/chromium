// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/biometric_authenticator_mac.h"

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#include "device/fido/mac/touch_id_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using MockAuthResultCallback =
    base::MockCallback<BiometricAuthenticatorMac::AuthenticateCallback>;

using device_reauth::BiometricAuthRequester;
using password_manager::PasswordAccessAuthenticator;

}  // namespace

class BiometricAuthenticatorMacTest : public testing::Test {
 public:
  device_reauth::BiometricAuthenticator* authenticator() {
    return authenticator_.get();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  device::fido::mac::ScopedTouchIdTestEnvironment* touch_id_enviroment() {
    return &touch_id_test_environment_;
  }

  MockAuthResultCallback& result_callback() { return result_callback_; }

  void ResetAuthenticator() { authenticator_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<device_reauth::BiometricAuthenticator> authenticator_ =
      ChromeBiometricAuthenticatorFactory::GetInstance()
          ->GetOrCreateBiometricAuthenticator();
  device::fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment_{
      config_};
  MockAuthResultCallback result_callback_;
};

// If time that passed since the last successful authentication is smaller than
// kAuthValidityPeriod, no reauthentication is needed.
TEST_F(BiometricAuthenticatorMacTest, NoReauthenticationIfLessThan60Seconds) {
  touch_id_enviroment()->SimulateTouchIdPromptSuccess();
  EXPECT_CALL(result_callback(), Run(/*success=*/true));

  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Since the delay is smaller than kAuthValidityPeriod there shouldn't be
  // another prompt, so the auth should be reported as successful. If there is a
  // call to touchIdContext test will fail as TouchIdEnviroment will crash since
  // there is no prompt expected.
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);

  EXPECT_CALL(result_callback(), Run(/*success=*/true));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());
}

// If the time since the last reauthentication is greater than
// kAuthValidityPeriod or the authentication failed, reauthentication is needed.
TEST_F(BiometricAuthenticatorMacTest, ReauthenticationIfMoreThan60Seconds) {
  touch_id_enviroment()->SimulateTouchIdPromptSuccess();
  EXPECT_CALL(result_callback(), Run(/*success=*/true));

  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Make the next touch ID prompt auth fail.
  touch_id_enviroment()->SimulateTouchIdPromptFailure();
  // Since the delay is bigger than kAuthValidityPeriod, the previous auth has
  // expired. Thus a new prompt will be requested which should fail the
  // authentication.
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod * 2);

  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());
}

// If prevoius authentication failed kAuthValidityPeriod isn't started and
// reauthentication will be needed.
TEST_F(BiometricAuthenticatorMacTest, ReauthenticationIfPreviousFailed) {
  touch_id_enviroment()->SimulateTouchIdPromptFailure();

  // First authetication fails, no last_good_auth_timestamp_ should be
  // recorded, which fill force reauthentication.
  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Although it passed less than kAuthValidityPeriod no valid authenticaion
  // should be recorded as pormptTouchId will fail.
  touch_id_enviroment()->SimulateTouchIdPromptFailure();
  task_environment().FastForwardBy(
      PasswordAccessAuthenticator::kAuthValidityPeriod / 2);

  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());
}

// If pending authentication can be canceled.
TEST_F(BiometricAuthenticatorMacTest, CancelPendngAuthentication) {
  touch_id_enviroment()->SimulateTouchIdPromptSuccess();
  touch_id_enviroment()->DoNotResolveNextPrompt();

  authenticator()->AuthenticateWithMessage(
      BiometricAuthRequester::kPasswordsInSettings,
      /*message=*/u"Chrome is trying to show passwords.",
      result_callback().Get());

  // Authentication should fail as it will take 10 seconds to authenticate, and
  // there will be a cancelation in the meantime.
  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  authenticator()->Cancel(BiometricAuthRequester::kPasswordsInSettings);
}
