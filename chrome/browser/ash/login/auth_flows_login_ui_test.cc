// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/auto_reset.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/fake_recovery_service_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "content/public/test/browser_test.h"

namespace ash {

using test::UserAuthConfig;

// Base class for UI testing of various authentication that can
// take place at the login screen.
class AuthFlowsLoginTestBase : public LoginManagerTest {
 public:
  explicit AuthFlowsLoginTestBase(bool require_reauth)
      : with_gaia_pw_{LoginManagerMixin::CreateConsumerAccountId(1),
                      UserAuthConfig::Create({AshAuthFactor::kGaiaPassword})
                          .RequireReauth(require_reauth)},
        with_gaia_pw_recovery_{
            LoginManagerMixin::CreateConsumerAccountId(2),
            UserAuthConfig::Create(
                {AshAuthFactor::kGaiaPassword, AshAuthFactor::kRecovery})
                .RequireReauth(require_reauth)},
        with_local_pw_{LoginManagerMixin::CreateConsumerAccountId(3),
                       UserAuthConfig::Create({AshAuthFactor::kLocalPassword})
                           .RequireReauth(require_reauth)},
        with_local_pw_recovery_{
            LoginManagerMixin::CreateConsumerAccountId(4),
            UserAuthConfig::Create(
                {AshAuthFactor::kLocalPassword, AshAuthFactor::kRecovery})
                .RequireReauth(require_reauth)},
        login_mixin_{&mixin_host_,
                     {with_gaia_pw_, with_gaia_pw_recovery_, with_local_pw_,
                      with_local_pw_recovery_},
                     &fake_gaia_,
                     &cryptohome_}

  {}
  ~AuthFlowsLoginTestBase() override = default;

  void SetUpOnMainThread() override {
    // Make `FakeUserDataAuthClient` perform actual password checks when
    // handling authentication requests.
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
    LoginManagerTest::SetUpOnMainThread();
  }

  void ConfigureFakeGaiaFor(const LoginManagerMixin::TestUserInfo& user) {
    fake_gaia_.SetupFakeGaiaForLogin(user.account_id.GetUserEmail(),
                                     user.account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
  }

 protected:
  const LoginManagerMixin::TestUserInfo with_gaia_pw_;
  const LoginManagerMixin::TestUserInfo with_gaia_pw_recovery_;
  const LoginManagerMixin::TestUserInfo with_local_pw_;
  const LoginManagerMixin::TestUserInfo with_local_pw_recovery_;

  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  base::AutoReset<bool> branded_build{&WizardContext::g_is_branded_build, true};
  LoginManagerMixin login_mixin_;
  FakeRecoveryServiceMixin fake_recovery_service_{&mixin_host_,
                                                  embedded_test_server()};
};

// ----------------------------------------------------------

class AuthFlowsLoginReauthTest : public AuthFlowsLoginTestBase {
 public:
  AuthFlowsLoginReauthTest()
      : AuthFlowsLoginTestBase(/* require_reauth */ true) {
  }
  ~AuthFlowsLoginReauthTest() override = default;

  void TriggerUserOnlineAuth(const LoginManagerMixin::TestUserInfo user,
                             const std::string& password) {
    ConfigureFakeGaiaFor(user);

    test::OnLoginScreen()->SelectUserPod(user.account_id);
    auto gaia = test::AwaitGaiaSigninUI();

    gaia->ReauthConfirmEmail(user.account_id);
    gaia->TypePassword(password);
    gaia->ContinueLogin();
  }
};

// ----------------------------------------------------------

class AuthFlowsLoginRecoverUserTest : public AuthFlowsLoginTestBase {
 public:
  AuthFlowsLoginRecoverUserTest()
      : AuthFlowsLoginTestBase(/* require_reauth */ false) {
  }

  ~AuthFlowsLoginRecoverUserTest() override = default;

  void TriggerRecoveryOnAuthErrorBubble(
      const LoginManagerMixin::TestUserInfo user) {
    ConfigureFakeGaiaFor(user);

    test::OnLoginScreen()->SelectUserPod(user.account_id);

    test::OnLoginScreen()->SubmitPassword(user.account_id,
                                          test::kWrongPassword);

    auto auth_error_bubble = test::OnLoginScreen()->WaitForAuthErrorBubble();

    auth_error_bubble->PressRecoveryButton();
    test::AuthErrorBubbleDismissWaiter()->Wait();
  }

  void TriggerUserOnlineAuth(const LoginManagerMixin::TestUserInfo user,
                             const std::string& password) {
    TriggerRecoveryOnAuthErrorBubble(user);

    // Use gaia to authenticate for recovery.
    auto gaia = test::AwaitGaiaSigninUI();

    gaia->ReauthConfirmEmail(user.account_id);
    gaia->TypePassword(password);
    gaia->ContinueLogin();
  }
};

// ----------------------------------------------------------

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, GaiaPasswordNotChanged) {
  const auto& user = with_gaia_pw_;

  TriggerUserOnlineAuth(user, test::kGaiaPassword);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest,
                       GaiaPasswordRecoveryNotChanged) {
  const auto& user = with_gaia_pw_recovery_;

  TriggerUserOnlineAuth(user, test::kGaiaPassword);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, GaiaPasswordChangedRecovery) {
  const auto& user = with_gaia_pw_recovery_;

  TriggerUserOnlineAuth(user, test::kNewPassword);

  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, GaiaPasswordChangedManual) {
  const auto& user = with_gaia_pw_;

  TriggerUserOnlineAuth(user, test::kNewPassword);

  auto pw_changed = test::AwaitPasswordChangedUI();
  pw_changed->TypePreviousPassword(test::kGaiaPassword);
  pw_changed->SubmitPreviousPassword();

  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

// ----------------------------------------------------------

class AuthFlowsLoginAddExistingUserTest : public AuthFlowsLoginTestBase {
 public:
  AuthFlowsLoginAddExistingUserTest()
      : AuthFlowsLoginTestBase(/* require_reauth */ false) {}
  ~AuthFlowsLoginAddExistingUserTest() override = default;

  void TriggerUserOnlineAuth(const LoginManagerMixin::TestUserInfo user,
                             const std::string& password) {
    ConfigureFakeGaiaFor(user);

    test::OnLoginScreen()->AddNewUser();
    auto new_user = test::AwaitNewUserSelectionUI();

    new_user->ChooseConsumerUser();
    new_user->AwaitNextButton();
    new_user->Next();

    auto gaia = test::AwaitGaiaSigninUI();

    gaia->SubmitFullAuthEmail(user.account_id);
    gaia->TypePassword(password);
    gaia->ContinueLogin();
  }

  void ExpectReauthForRapt(const LoginManagerMixin::TestUserInfo user,
                           const std::string& password) {
    auto reauth = test::AwaitRecoveryReauthUI();
    reauth->ConfirmReauth();

    auto gaia = test::AwaitGaiaSigninUI();

    gaia->ReauthConfirmEmail(user.account_id);
    gaia->TypePassword(password);
    gaia->ContinueLogin();
  }
};

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginAddExistingUserTest,
                       GaiaPasswordNotChanged) {
  const auto& user = with_gaia_pw_;

  TriggerUserOnlineAuth(user, test::kGaiaPassword);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginAddExistingUserTest,
                       GaiaPassworRecoverydNotChanged) {
  const auto& user = with_gaia_pw_recovery_;

  TriggerUserOnlineAuth(user, test::kGaiaPassword);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginAddExistingUserTest,
                       GaiaPasswordChangedRecovery) {
  const auto& user = with_gaia_pw_recovery_;

  TriggerUserOnlineAuth(user, test::kNewPassword);

  // Add new user flow does not provide RAPT token required for recovery.
  // Expect a request to reauthenticate and proceed with recovery.
  ExpectReauthForRapt(user, test::kNewPassword);

  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginAddExistingUserTest,
                       GaiaPasswordChangedManual) {
  const auto& user = with_gaia_pw_;

  TriggerUserOnlineAuth(user, test::kNewPassword);

  auto pw_changed = test::AwaitPasswordChangedUI();
  pw_changed->TypePreviousPassword(test::kGaiaPassword);
  pw_changed->SubmitPreviousPassword();

  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

// ----------------------------------------------------------

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest,
                       GaiaPasswordChangedWithRecoveryLocalPassword) {
  const auto& user = with_local_pw_recovery_;

  test::OnLoginScreen()->SelectUserPod(user.account_id);
  auto gaia = test::AwaitGaiaSigninUI();

  gaia->ReauthConfirmEmail(user.account_id);
  gaia->TypePassword(test::kNewPassword);
  gaia->ContinueLogin();

  auto local_authentication =
      test::OnLoginScreen()->WaitForLocalAuthenticationDialog();

  local_authentication->SubmitPassword(test::kLocalPassword);
  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, LocalPasswordCorrectPassword) {
  const auto& user = with_local_pw_;

  test::OnLoginScreen()->SelectUserPod(user.account_id);
  auto gaia = test::AwaitGaiaSigninUI();

  gaia->ReauthConfirmEmail(user.account_id);
  gaia->TypePassword(test::kNewPassword);
  gaia->ContinueLogin();

  auto local_authentication =
      test::OnLoginScreen()->WaitForLocalAuthenticationDialog();

  local_authentication->SubmitPassword(test::kLocalPassword);
  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest,
                       CancelLocalAuthenticationDialog) {
  const auto& user = with_local_pw_;

  TriggerUserOnlineAuth(user, test::kGaiaPassword);

  auto local_authentication =
      test::OnLoginScreen()->WaitForLocalAuthenticationDialog();

  local_authentication->CancelDialog();

  local_authentication->WaitUntilDismissed();

  // After dismissing the local authentication dialog,
  // the flow should return to and display the login screen.
  test::OnLoginScreen()->SelectUserPod(user.account_id);
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, AuthenticateWithRecovery) {
  const auto& user = with_local_pw_recovery_;

  TriggerUserOnlineAuth(user, test::kGaiaPassword);

  // After successful GAIA authentication in recovery session,
  // the session should start automatically.
  login_mixin_.WaitForActiveSession();
}

// ----------------------------------------------------------

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginRecoverUserTest,
                       LocalPasswordWithRecovery) {
  const auto& user = with_local_pw_recovery_;

  TriggerUserOnlineAuth(user, FakeGaiaMixin::kFakeUserPassword);

  // Set up a new password.
  auto local_password_setup = test::AwaitLocalPasswordSetupUI();

  local_password_setup->TypeFirstPassword(test::kNewPassword);
  local_password_setup->TypeConfirmPassword(test::kNewPassword);
  local_password_setup->Submit();

  // Recovery password update confirmation.
  test::RecoveryPasswordUpdatedPageWaiter()->Wait();
  test::RecoveryPasswordUpdatedProceedAction();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginRecoverUserTest,
                       LocalPasswordWithoutRecoveryCancelLAD) {
  const auto& user = with_local_pw_;
  // Start recovery flow without recovery auth factor.
  TriggerUserOnlineAuth(user, FakeGaiaMixin::kFakeUserPassword);

  // Wait for local data loss warning.
  test::LocalDataLossWarningPageWaiter()->Wait();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginRecoverUserTest,
                       GaiaPasswordWithRecovery) {
  const auto& user = with_gaia_pw_recovery_;

  // Start recovery flow with recovery auth factor.
  TriggerUserOnlineAuth(user, test::kNewPassword);

  // Wait for password update dialog.
  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginRecoverUserTest,
                       GaiaPasswordWithoutRecovery) {
  const auto& user = with_gaia_pw_;

  // Start recovery flow without recovery auth factor.
  TriggerUserOnlineAuth(user, test::kWrongPassword);

  auto pw_changed = test::AwaitPasswordChangedUI();
  pw_changed->TypePreviousPassword(test::kGaiaPassword);
  pw_changed->SubmitPreviousPassword();

  // Wait for password update dialog.
  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginRecoverUserTest,
                       GaiaPasswordWithoutRecoveryInvalidPassword) {
  const auto& user = with_gaia_pw_;

  // Start recovery flow without recovery auth factor.
  TriggerUserOnlineAuth(user, test::kWrongPassword);

  auto pw_changed = test::AwaitPasswordChangedUI();
  pw_changed->TypePreviousPassword(test::kWrongPassword);
  pw_changed->SubmitPreviousPassword();

  // Keep user on passwordChanged screen after incorrect password, display error
  // message.
  pw_changed->InvalidPasswordFeedback()->Wait();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginRecoverUserTest,
                       GaiaPasswordWithoutRecoveryForgotPasswordClick) {
  const auto& user = with_gaia_pw_;

  // Start recovery flow without recovery auth factor.
  TriggerUserOnlineAuth(user, test::kWrongPassword);

  auto pw_changed = test::AwaitPasswordChangedUI();
  // Clicks on forgot password button.
  pw_changed->ForgotPreviousPassword();

  // Wait for local data loss warning.
  test::LocalDataLossWarningPageWaiter()->Wait();
}

}  // namespace ash
