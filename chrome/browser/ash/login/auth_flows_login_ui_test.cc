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
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
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
};

class AuthFlowsLoginReauthTest : public AuthFlowsLoginTestBase {
 public:
  AuthFlowsLoginReauthTest()
      : AuthFlowsLoginTestBase(/* require_reauth */ true) {}
  ~AuthFlowsLoginReauthTest() override = default;
};

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, GaiaPasswordNotChanged) {
  ConfigureFakeGaiaFor(with_gaia_pw_);

  test::OnLoginScreen()->SelectUserPod(with_gaia_pw_.account_id);
  auto gaia = test::AwaitGaiaSigninUI();

  gaia->ReauthConfirmEmail(with_gaia_pw_.account_id);
  gaia->TypePassword(test::kGaiaPassword);
  gaia->ContinueLogin();

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(AuthFlowsLoginReauthTest, GaiaPasswordChangedManual) {
  ConfigureFakeGaiaFor(with_gaia_pw_);

  test::OnLoginScreen()->SelectUserPod(with_gaia_pw_.account_id);
  auto gaia = test::AwaitGaiaSigninUI();

  gaia->ReauthConfirmEmail(with_gaia_pw_.account_id);
  gaia->TypePassword(test::kNewPassword);
  gaia->ContinueLogin();

  auto pw_changed = test::AwaitPasswordChangedUI();
  pw_changed->TypePreviousPassword(test::kGaiaPassword);
  pw_changed->SubmitPreviousPassword();

  auto pw_updated = test::AwaitPasswordUpdatedUI();
  pw_updated->ExpectPasswordUpdateState();
  pw_updated->ConfirmPasswordUpdate();

  login_mixin_.WaitForActiveSession();
}

}  // namespace ash
