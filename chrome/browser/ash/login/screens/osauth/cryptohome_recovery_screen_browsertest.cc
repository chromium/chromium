// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/fake_recovery_service_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/osauth/local_data_loss_warning_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
const char kNewPassword[] = "new user password";
}  // namespace

class CryptohomeRecoveryScreenTestBase : public OobeBaseTest {
 public:
  explicit CryptohomeRecoveryScreenTestBase(
      const LoginManagerMixin::TestUserInfo& test_user)
      : test_user_(test_user) {}

  ~CryptohomeRecoveryScreenTestBase() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    // Make `FakeUserDataAuthClient` perform actual password checks when
    // handling authentication requests. This is necessary for triggering the
    // password change UI flow.
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
  }

  void SetupFakeGaia(const LoginManagerMixin::TestUserInfo& user) {
    fake_gaia_.SetupFakeGaiaForLogin(user.account_id.GetUserEmail(),
                                     user.account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
  }

  void SetGaiaScreenCredentials(const AccountId& account_id,
                                const std::string& password) {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(account_id.GetUserEmail(), password,
                                  FakeGaiaMixin::kEmptyUserServices);
  }

  void OpenGaiaDialog(const AccountId& account_id) {
    EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
    EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));
    EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    result_ = std::nullopt;
  }

  void SetUpExitCallback() {
    CryptohomeRecoveryScreen* screen =
        WizardController::default_controller()
            ->GetScreen<CryptohomeRecoveryScreen>();
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(
        base::BindRepeating(&CryptohomeRecoveryScreenTestBase::HandleScreenExit,
                            base::Unretained(this)));
  }

  bool FireExpirationTimer() {
    CryptohomeRecoveryScreen* screen =
        WizardController::default_controller()
            ->GetScreen<CryptohomeRecoveryScreen>();
    auto* timer = screen->get_timer_for_testing();
    if (!timer) {
      return false;
    }
    timer->FireNow();
    return true;
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool IsMounted() {
    base::test::TestFuture<std::optional<user_data_auth::IsMountedReply>>
        future;
    FakeUserDataAuthClient::Get()->IsMounted(user_data_auth::IsMountedRequest(),
                                             future.GetCallback());
    auto mount_result = future.Get();
    CHECK(mount_result.has_value());
    return mount_result->is_mounted();
  }

 protected:
  const LoginManagerMixin::TestUserInfo test_user_;
  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_,
                                         {test_user_},
                                         &fake_gaia_,
                                         &cryptohome_};
  FakeRecoveryServiceMixin fake_recovery_service_{&mixin_host_,
                                                  embedded_test_server()};
  std::optional<CryptohomeRecoveryScreen::Result> result_;

 private:
  void HandleScreenExit(CryptohomeRecoveryScreen::Result result) {
    result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  CryptohomeRecoveryScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

class CryptohomeRecoveryScreenTest : public CryptohomeRecoveryScreenTestBase {
 public:
  CryptohomeRecoveryScreenTest()
      : CryptohomeRecoveryScreenTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                           FakeGaiaMixin::kFakeUserGaiaId),
            test::UserAuthConfig::Create({ash::AshAuthFactor::kGaiaPassword,
                                          ash::AshAuthFactor::kRecovery})
                .RequireReauth()}) {}
  ~CryptohomeRecoveryScreenTest() override = default;

  CryptohomeRecoveryScreenTest(const CryptohomeRecoveryScreenTest& other) =
      delete;
  CryptohomeRecoveryScreenTest& operator=(
      const CryptohomeRecoveryScreenTest& other) = delete;
};

class CryptohomeRecoveryScreenNoRecoveryTest
    : public CryptohomeRecoveryScreenTestBase {
 public:
  CryptohomeRecoveryScreenNoRecoveryTest()
      : CryptohomeRecoveryScreenTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                           FakeGaiaMixin::kFakeUserGaiaId),
            test::UserAuthConfig::Create(test::kDefaultAuthSetup)
                .RequireReauth()}) {}
  ~CryptohomeRecoveryScreenNoRecoveryTest() override = default;

  CryptohomeRecoveryScreenNoRecoveryTest(
      const CryptohomeRecoveryScreenNoRecoveryTest& other) = delete;
  CryptohomeRecoveryScreenNoRecoveryTest& operator=(
      const CryptohomeRecoveryScreenNoRecoveryTest& other) = delete;
};

// Successful recovery after password change is detected.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest, SuccessfulRecovery) {
  SetupFakeGaia(test_user_);

  OpenGaiaDialog(test_user_.account_id);

  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContext()
                ->gaia_config.gaia_path,
            WizardContext::GaiaPath::kReauth);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kAuthenticated);

  test::RecoveryPasswordUpdatedPageWaiter()->Wait();
  test::RecoveryPasswordUpdatedProceedAction();

  OobeWindowVisibilityWaiter(false).Wait();
  login_manager_mixin_.WaitForActiveSession();
  EXPECT_TRUE(IsMounted());
}

// Verifies that recovery is skipped and GaiaPasswordChangedScreen is shown when
// recovery factor is not configured.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenNoRecoveryTest,
                       NoRecoveryFactor) {
  SetupFakeGaia(test_user_);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContext()
                ->gaia_config.gaia_path,
            WizardContext::GaiaPath::kReauth);

  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kFallbackOnline);
  OobeScreenWaiter(EnterOldPasswordScreenView::kScreenId).Wait();
  EXPECT_FALSE(IsMounted());
}

// Verifies that right reset password screen is shows and we goto Useronboarding
// flow.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenNoRecoveryTest, ResetSuccess) {
  SetupFakeGaia(test_user_);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContext()
                ->gaia_config.gaia_path,
            WizardContext::GaiaPath::kReauth);

  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();

  WaitForScreenExit();

  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kFallbackOnline);
  OobeScreenWaiter(EnterOldPasswordScreenView::kScreenId).Wait();
  test::CreateOldPasswordEnterPageWaiter()->Wait();

  // Click forgot password button.
  test::PasswordChangedForgotPasswordAction();
  test::LocalDataLossWarningPageWaiter()->Wait();

  test::LocalDataLossWarningPageExpectGoBack();
  test::LocalDataLossWarningPageExpectRemove();

  // Click "Proceed anyway".
  test::LocalDataLossWarningPageRemoveAction();

  WizardControllerExitWaiter(LocalDataLossWarningScreenView::kScreenId).Wait();

  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContext()
                ->knowledge_factor_setup.auth_setup_flow,
            WizardContext::AuthChangeFlow::kInitialSetup);

  // Following this we will wait for password entry.
}

// Verifies that we could fallback to the manual recovery when there is error
// during recovery.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest, ManualRecoveryAfterError) {
  SetupFakeGaia(test_user_);
  fake_recovery_service_.SetErrorResponse("/v1/cryptorecovery",
                                          net::HTTP_BAD_REQUEST);

  OpenGaiaDialog(test_user_.account_id);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kError);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  EXPECT_FALSE(IsMounted());
}

class CryptohomeRecoveryScreenChildTest
    : public CryptohomeRecoveryScreenTestBase {
 public:
  CryptohomeRecoveryScreenChildTest()
      : CryptohomeRecoveryScreenTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                           FakeGaiaMixin::kFakeUserGaiaId),
            test::UserAuthConfig::Create({ash::AshAuthFactor::kGaiaPassword,
                                          ash::AshAuthFactor::kRecovery})
                .RequireReauth(),
            user_manager::UserType::kChild}) {}
  ~CryptohomeRecoveryScreenChildTest() override = default;

  CryptohomeRecoveryScreenChildTest(
      const CryptohomeRecoveryScreenChildTest& other) = delete;
  CryptohomeRecoveryScreenChildTest& operator=(
      const CryptohomeRecoveryScreenChildTest& other) = delete;
};

class CryptohomeRecoveryScreenChildNoRecoveryTest
    : public CryptohomeRecoveryScreenTestBase {
 public:
  CryptohomeRecoveryScreenChildNoRecoveryTest()
      : CryptohomeRecoveryScreenTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                           FakeGaiaMixin::kFakeUserGaiaId),
            test::UserAuthConfig::Create(test::kDefaultAuthSetup)
                .RequireReauth(),
            user_manager::UserType::kChild}) {}
  ~CryptohomeRecoveryScreenChildNoRecoveryTest() override = default;

  CryptohomeRecoveryScreenChildNoRecoveryTest(
      const CryptohomeRecoveryScreenChildNoRecoveryTest& other) = delete;
  CryptohomeRecoveryScreenChildNoRecoveryTest& operator=(
      const CryptohomeRecoveryScreenChildNoRecoveryTest& other) = delete;
};

// Successful recovery after password change is detected for child users.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenChildTest, SuccessfulRecovery) {
  SetupFakeGaia(test_user_);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContext()
                ->gaia_config.gaia_path,
            WizardContext::GaiaPath::kReauth);

  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kAuthenticated);

  test::RecoveryPasswordUpdatedPageWaiter()->Wait();
  test::RecoveryPasswordUpdatedProceedAction();

  OobeWindowVisibilityWaiter(false).Wait();
  login_manager_mixin_.WaitForActiveSession();
}

// Verifies that recovery is skipped for child users when recovery factor is not
// configured.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenChildNoRecoveryTest,
                       NoRecoveryFactor) {
  SetupFakeGaia(test_user_);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContext()
                ->gaia_config.gaia_path,
            WizardContext::GaiaPath::kReauth);

  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();

  WaitForScreenExit();

  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kFallbackOnline);
  OobeScreenWaiter(EnterOldPasswordScreenView::kScreenId).Wait();
}

}  // namespace ash
