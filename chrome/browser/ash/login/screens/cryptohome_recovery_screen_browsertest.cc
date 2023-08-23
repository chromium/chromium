// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/cryptohome_recovery_setup_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/fake_recovery_service_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

const test::UIPath kSuccessStep = {"cryptohome-recovery", "successDialog"};
const test::UIPath kErrorStep = {"cryptohome-recovery", "errorDialog"};
const test::UIPath kReauthNotificationStep = {"cryptohome-recovery",
                                              "reauthNotificationDialog"};
const test::UIPath kDoneButton = {"cryptohome-recovery", "doneButton"};
const test::UIPath kManualRecoveryButton = {"cryptohome-recovery",
                                            "manualRecoveryButton"};
const test::UIPath kRetryButton = {"cryptohome-recovery", "retryButton"};
const test::UIPath kReauthButton = {"cryptohome-recovery", "reauthButton"};

const char kOldPassword[] = "old user password";
const char kNewPassword[] = "new user password";
}  // namespace

class CryptohomeRecoveryScreenTestBase : public OobeBaseTest {
 public:
  explicit CryptohomeRecoveryScreenTestBase(
      const LoginManagerMixin::TestUserInfo& test_user)
      : test_user_(test_user) {
    feature_list_.InitAndEnableFeature(features::kCryptohomeRecovery);
  }

  ~CryptohomeRecoveryScreenTestBase() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    // Make `FakeUserDataAuthClient` perform actual password checks when
    // handling authentication requests. This is necessary for triggering the
    // password change UI flow.
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
  }

  void AddFakeUser(const std::string& password) {
    cryptohome_.MarkUserAsExisting(test_user_.account_id);
    cryptohome_.AddGaiaPassword(test_user_.account_id, password);
    fake_gaia_.SetupFakeGaiaForLogin(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId,
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
    result_ = absl::nullopt;
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

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
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
  absl::optional<CryptohomeRecoveryScreen::Result> result_;
  base::test::ScopedFeatureList feature_list_;

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
            user_manager::UserType::USER_TYPE_REGULAR,
            user_manager::User::OAuthTokenStatus::
                OAUTH2_TOKEN_STATUS_INVALID}) {}
  ~CryptohomeRecoveryScreenTest() override = default;

  CryptohomeRecoveryScreenTest(const CryptohomeRecoveryScreenTest& other) =
      delete;
  CryptohomeRecoveryScreenTest& operator=(
      const CryptohomeRecoveryScreenTest& other) = delete;
};

// Successful recovery after password change is detected.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest, SuccessfulRecovery) {
  AddFakeUser(kOldPassword);
  cryptohome_.AddRecoveryFactor(test_user_.account_id);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetOobeUI()
                ->GetHandler<GaiaScreenHandler>()
                ->GetGaiaPath(),
            GaiaScreenHandler::GaiaPath::kReauth);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSuccessStep)->Wait();
  test::OobeJS().ClickOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kSucceeded);

  OobeWindowVisibilityWaiter(false).Wait();
  login_manager_mixin_.WaitForActiveSession();
}

// Verifies that recovery is skipped and GaiaPasswordChangedScreen is shown when
// recovery factor is not configured.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest, NoRecoveryFactor) {
  AddFakeUser(kOldPassword);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetOobeUI()
                ->GetHandler<GaiaScreenHandler>()
                ->GetGaiaPath(),
            GaiaScreenHandler::GaiaPath::kDefault);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();

  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            CryptohomeRecoveryScreen::Result::kNoRecoveryFactor);
  OobeScreenWaiter(GaiaPasswordChangedView::kScreenId).Wait();
}

// Verifies that we could fallback to the manual recovery when there is error
// during recovery.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest, ManualRecoveryAfterError) {
  AddFakeUser(kOldPassword);
  cryptohome_.AddRecoveryFactor(test_user_.account_id);
  fake_recovery_service_.SetErrorResponse("/v1/rart",
                                          net::HTTP_SERVICE_UNAVAILABLE);

  OpenGaiaDialog(test_user_.account_id);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kErrorStep)->Wait();
  test::OobeJS().ClickOnPath(kManualRecoveryButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kManualRecovery);
  OobeScreenWaiter(GaiaPasswordChangedView::kScreenId).Wait();
}

// Verifies that we could retry when there is error during recovery.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest, RetryAfterError) {
  AddFakeUser(kOldPassword);
  cryptohome_.AddRecoveryFactor(test_user_.account_id);
  fake_recovery_service_.SetErrorResponse("/v1/cryptorecovery",
                                          net::HTTP_BAD_REQUEST);

  OpenGaiaDialog(test_user_.account_id);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kErrorStep)->Wait();
  test::OobeJS().ClickOnPath(kRetryButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kRetry);

  fake_recovery_service_.SetErrorResponse("/v1/cryptorecovery", net::HTTP_OK);

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSuccessStep)->Wait();
  test::OobeJS().ClickOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kSucceeded);

  OobeWindowVisibilityWaiter(false).Wait();
  login_manager_mixin_.WaitForActiveSession();
}

// Verifies that user is asked to sign in again when reauth token is not present
// when password change is detected.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenTest,
                       MissingReauthTokenDuringRecovery) {
  AddFakeUser(kOldPassword);
  cryptohome_.AddRecoveryFactor(test_user_.account_id);

  // Entering the add person flow with an existing account. Reauth token was not
  // fetched in this case.
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kReauthNotificationStep)->Wait();
  test::OobeJS().ClickOnPath(kReauthButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kGaiaLogin);

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSuccessStep)->Wait();
  test::OobeJS().ClickOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kSucceeded);

  OobeWindowVisibilityWaiter(false).Wait();
  login_manager_mixin_.WaitForActiveSession();
}

class CryptohomeRecoveryScreenChildTest
    : public CryptohomeRecoveryScreenTestBase {
 public:
  CryptohomeRecoveryScreenChildTest()
      : CryptohomeRecoveryScreenTestBase(LoginManagerMixin::TestUserInfo{
            AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                           FakeGaiaMixin::kFakeUserGaiaId),
            user_manager::UserType::USER_TYPE_CHILD,
            user_manager::User::OAuthTokenStatus::
                OAUTH2_TOKEN_STATUS_INVALID}) {}
  ~CryptohomeRecoveryScreenChildTest() override = default;

  CryptohomeRecoveryScreenChildTest(
      const CryptohomeRecoveryScreenChildTest& other) = delete;
  CryptohomeRecoveryScreenChildTest& operator=(
      const CryptohomeRecoveryScreenChildTest& other) = delete;
};

// Successful recovery after password change is detected for child users.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenChildTest, SuccessfulRecovery) {
  AddFakeUser(kOldPassword);
  cryptohome_.AddRecoveryFactor(test_user_.account_id);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetOobeUI()
                ->GetHandler<GaiaScreenHandler>()
                ->GetGaiaPath(),
            GaiaScreenHandler::GaiaPath::kReauth);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kSuccessStep)->Wait();
  test::OobeJS().ClickOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoveryScreen::Result::kSucceeded);

  OobeWindowVisibilityWaiter(false).Wait();
  login_manager_mixin_.WaitForActiveSession();
}

// Verifies that recovery is skipped for child users when recovery factor is not
// configured.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoveryScreenChildTest, NoRecoveryFactor) {
  AddFakeUser(kOldPassword);

  OpenGaiaDialog(test_user_.account_id);
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetOobeUI()
                ->GetHandler<GaiaScreenHandler>()
                ->GetGaiaPath(),
            GaiaScreenHandler::GaiaPath::kReauth);
  SetUpExitCallback();
  SetGaiaScreenCredentials(test_user_.account_id, kNewPassword);

  OobeScreenWaiter(CryptohomeRecoveryScreenView::kScreenId).Wait();

  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            CryptohomeRecoveryScreen::Result::kNoRecoveryFactor);
  OobeScreenWaiter(GaiaPasswordChangedView::kScreenId).Wait();
}

}  // namespace ash
