// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/reauth_reason.h"
#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "test-user@gmail.com";
constexpr char kGaiaID[] = "111111";
constexpr char kTokenHandle[] = "test_token_handle";
constexpr char kTestingFileName[] = "testing-file.txt";

using AuthOp = FakeUserDataAuthClient::Operation;

}  // namespace

class PasswordChangeTestBase : public LoginManagerTest {
 public:
  PasswordChangeTestBase() = default;
  ~PasswordChangeTestBase() override = default;

 protected:
  void OpenGaiaDialog(const AccountId& account_id) {
    EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
    EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));
    EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  }

  void ExpectButtonsState() {
    EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
    EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
    EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  }

 protected:
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID);
  const LoginManagerMixin::TestUserInfo test_user_info_{
      test_account_id_,
      test::UserAuthConfig::Create(test::kDefaultAuthSetup).RequireReauth()};
};

// Test fixture that uses a fake UserDataAuth in order to simulate password
// change flows.
class PasswordChangeTest : public PasswordChangeTestBase {
 protected:
  PasswordChangeTest() = default;

  void SetUpOnMainThread() override {
    // Make `FakeUserDataAuthClient` perform actual password checks when
    // handling authentication requests. This is necessary for triggering the
    // password change UI flow.
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
    PasswordChangeTestBase::SetUpOnMainThread();
    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
  }

  bool TestingFileExists() const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(GetTestingFilePath());
  }

  void CreateTestingFile() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(GetTestingFilePath(), /*data=*/""));
  }

  void SetGaiaScreenCredentials(const AccountId& account_id,
                                const std::string& password) {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(account_id.GetUserEmail(), password,
                                  FakeGaiaMixin::kEmptyUserServices);
  }

  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_,
                                 {test_user_info_},
                                 &fake_gaia_,
                                 &cryptohome_};
  base::AutoReset<bool> branded_build{&WizardContext::g_is_branded_build, true};

 private:
  base::FilePath GetTestingFilePath() const {
    auto account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    std::optional<base::FilePath> profile_dir =
        FakeUserDataAuthClient::TestApi::Get()->GetUserProfileDir(
            account_identifier);
    if (!profile_dir) {
      ADD_FAILURE() << "Failed to get user profile dir";
      return base::FilePath();
    }
    return profile_dir.value().AppendASCII(kTestingFileName);
  }
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, UpdateGaiaPassword) {
  CreateTestingFile();
  OpenGaiaDialog(test_account_id_);

  base::HistogramTester histogram_tester;
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kOther, 1);

  // Fill out and submit the old password.
  test::PasswordChangedTypeOldPassword(
      test_user_info_.auth_config.online_password);
  test::PasswordChangedSubmitOldPassword();

  test::CreatePasswordUpdateNoticePageWaiter()->Wait();
  test::PasswordUpdateNoticeExpectDone();
  test::PasswordUpdateNoticeDoneAction();

  // User session should start, and whole OOBE screen is expected to be hidden.
  OobeWindowVisibilityWaiter(false).Wait();

  login_mixin_.WaitForActiveSession();
  EXPECT_TRUE(TestingFileExists());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, SubmitOnEnterKeyPressed) {
  OpenGaiaDialog(test_account_id_);

  base::HistogramTester histogram_tester;
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);
  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kOther, 1);

  // Fill out and submit the old password, using "ENTER" key.
  test::PasswordChangedTypeOldPassword(
      test_user_info_.auth_config.online_password);
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_RETURN, false /* control */, false /* shift */,
      false /* alt */, false /* command */));

  test::CreatePasswordUpdateNoticePageWaiter()->Wait();
  test::PasswordUpdateNoticeExpectDone();
  test::PasswordUpdateNoticeDoneAction();

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_TRUE(
      FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kUpdateAuthFactor>());

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, RetryOnWrongPassword) {
  CreateTestingFile();
  OpenGaiaDialog(test_account_id_);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  // Fill out and submit the old password passed to the fake userdataauth.
  test::PasswordChangedTypeOldPassword("incorrect old user password");
  test::PasswordChangedSubmitOldPassword();
  // Expect the UI to report failure, but stay on the same page.
  test::PasswordChangedInvalidPasswordFeedback()->Wait();
  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  // Submit the correct password.
  test::PasswordChangedTypeOldPassword(
      test_user_info_.auth_config.online_password);
  test::PasswordChangedSubmitOldPassword();

  test::CreatePasswordUpdateNoticePageWaiter()->Wait();
  test::PasswordUpdateNoticeExpectDone();
  test::PasswordUpdateNoticeDoneAction();

  // User session should start, and whole OOBE screen is expected to be hidden.
  OobeWindowVisibilityWaiter(false).Wait();
  login_mixin_.WaitForActiveSession();
  EXPECT_TRUE(TestingFileExists());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, SkipDataRecovery) {
  CreateTestingFile();
  OpenGaiaDialog(test_account_id_);
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  // Click forgot password button.
  test::PasswordChangedForgotPasswordAction();
  test::LocalDataLossWarningPageWaiter()->Wait();

  test::LocalDataLossWarningPageExpectGoBack();
  test::LocalDataLossWarningPageExpectRemove();

  // Click "Proceed anyway".
  test::LocalDataLossWarningPageRemoveAction();

  // With cryptohome recovery we re-create session and re-run onboarding.
  test::UserOnboardingWaiter()->Wait();

  EXPECT_FALSE(TestingFileExists());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, TryAgainAfterForgetLinkClick) {
  OpenGaiaDialog(test_account_id_);
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  // Click forgot password button.
  test::PasswordChangedForgotPasswordAction();
  test::LocalDataLossWarningPageWaiter()->Wait();

  test::LocalDataLossWarningPageExpectGoBack();
  test::LocalDataLossWarningPageExpectRemove();

  // Go back to old password input by clicking Try Again.
  test::LocalDataLossWarningPageGoBackAction();

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  // Enter and submit the correct password.
  test::PasswordChangedTypeOldPassword(
      test_user_info_.auth_config.online_password);
  test::PasswordChangedSubmitOldPassword();

  test::CreatePasswordUpdateNoticePageWaiter()->Wait();
  test::PasswordUpdateNoticeExpectDone();
  test::PasswordUpdateNoticeDoneAction();

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_TRUE(
      FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kUpdateAuthFactor>());

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, ClosePasswordChangedDialog) {
  OpenGaiaDialog(test_account_id_);
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
  ExpectButtonsState();

  test::PasswordChangedTypeOldPassword(
      test_user_info_.auth_config.online_password);
  // Switch to "Forgot password" step.
  test::PasswordChangedForgotPasswordAction();

  test::LocalDataLossWarningPageWaiter()->Wait();
  test::LocalDataLossWarningPageCancelAction();
  // Click the close button.

  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_FALSE(
      FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kUpdateAuthFactor>());

  OpenGaiaDialog(test_account_id_);
  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();
}

class PasswordChangeTokenCheck : public PasswordChangeTest {
 public:
  PasswordChangeTokenCheck() {
    login_mixin_.AppendRegularUsers(1);
    user_with_invalid_token_ = login_mixin_.users().back().account_id;
    ignore_sync_errors_for_test_ =
        SigninErrorNotifier::IgnoreSyncErrorsForTesting();
  }

 protected:
  // PasswordChangeTest:
  void SetUpInProcessBrowserTestFixture() override {
    PasswordChangeTest::SetUpInProcessBrowserTestFixture();
    TokenHandleUtil::SetInvalidTokenForTesting(kTokenHandle);
  }
  void TearDownInProcessBrowserTestFixture() override {
    TokenHandleUtil::SetInvalidTokenForTesting(nullptr);
    PasswordChangeTest::TearDownInProcessBrowserTestFixture();
  }

  AccountId user_with_invalid_token_;
  std::unique_ptr<base::AutoReset<bool>> ignore_sync_errors_for_test_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenPasswordChange) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);

  EXPECT_FALSE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));
  // Focus triggers token check.
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));

  OpenGaiaDialog(user_with_invalid_token_);

  base::HistogramTester histogram_tester;

  SetGaiaScreenCredentials(user_with_invalid_token_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();

  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kInvalidTokenHandle, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenNoPasswordChange) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Focus triggers token check.
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);

  OpenGaiaDialog(user_with_invalid_token_);
  base::HistogramTester histogram_tester;
  // Does not trigger password change screen.
  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());
  login_mixin_.WaitForActiveSession();
  histogram_tester.ExpectBucketCount("Login.PasswordNotChanged.ReauthReason",
                                     ReauthReason::kInvalidTokenHandle, 1);
}

// Helper class to create NotificationDisplayServiceTester before notification
// in the session shown.
class ProfileWaiter : public ProfileManagerObserver {
 public:
  ProfileWaiter() { g_browser_process->profile_manager()->AddObserver(this); }
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    g_browser_process->profile_manager()->RemoveObserver(this);
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile);
    run_loop_.Quit();
  }

  std::unique_ptr<NotificationDisplayServiceTester> Wait() {
    run_loop_.Run();
    return std::move(display_service_);
  }

 private:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  base::RunLoop run_loop_;
};

// Tests token handle check on the session start.
IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, PRE_Session) {
  // Focus triggers token check. User does not have stored token, so online
  // login should not be forced.
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);
  ASSERT_FALSE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));

  // Store invalid token to triger notification in the session.
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Make token not "checked recently".
  TokenHandleUtil::SetLastCheckedPrefForTesting(user_with_invalid_token_,
                                                base::Time());

  ProfileWaiter waiter;
  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());
  // We need to replace notification service very early to intercept reauth
  // notification.
  auto display_service_tester = waiter.Wait();

  login_mixin_.WaitForActiveSession();

  std::vector<message_center::Notification> notifications =
      display_service_tester->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1u);

  // Click on notification should trigger Chrome restart.
  base::RunLoop exit_waiter;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(exit_waiter.QuitClosure());

  display_service_tester->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                        notifications[0].id(), std::nullopt,
                                        std::nullopt);
  exit_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, Session) {
  ASSERT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));
  OpenGaiaDialog(user_with_invalid_token_);

  base::HistogramTester histogram_tester;

  SetGaiaScreenCredentials(user_with_invalid_token_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();

  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kInvalidTokenHandle, 1);
}

// Notification should not be triggered because token was checked on the login
// screen - recently.
IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, TokenRecentlyChecked) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);

  // Focus triggers token check and opens online
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);
  OpenGaiaDialog(user_with_invalid_token_);

  ProfileWaiter waiter;
  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());
  // We need to replace notification service very early to intercept reauth
  // notification.
  auto display_service_tester = waiter.Wait();

  login_mixin_.WaitForActiveSession();

  std::vector<message_center::Notification> notifications =
      display_service_tester->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 0u);
}

class TokenAfterCrash : public MixinBasedInProcessBrowserTest {
 public:
  TokenAfterCrash() {
    login_mixin_.set_session_restore_enabled();
    login_mixin_.SetShouldObtainHandle(true);
    login_mixin_.AppendRegularUsers(1);
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

// Test that token handle is downloaded on browser crash.
IN_PROC_BROWSER_TEST_F(TokenAfterCrash, PRE_NoToken) {
  auto user_info = login_mixin_.users()[0];
  login_mixin_.LoginWithDefaultContext(user_info);
  login_mixin_.WaitForActiveSession();

  EXPECT_TRUE(UserSessionManager::GetInstance()
                  ->token_handle_backfill_tried_for_testing());
  // Token should not be there as there are no real auth data.
  EXPECT_TRUE(TokenHandleUtil::ShouldObtainHandle(user_info.account_id));
}

IN_PROC_BROWSER_TEST_F(TokenAfterCrash, NoToken) {
  auto user_info = login_mixin_.users()[0];
  EXPECT_TRUE(UserSessionManager::GetInstance()
                  ->token_handle_backfill_tried_for_testing());
  // Token should not be there as there are no real auth data.
  EXPECT_TRUE(TokenHandleUtil::ShouldObtainHandle(user_info.account_id));
}

// Test that token handle is not downloaded on browser crash because it's
// already there.
IN_PROC_BROWSER_TEST_F(TokenAfterCrash, PRE_ValidToken) {
  auto user_info = login_mixin_.users()[0];
  login_mixin_.LoginWithDefaultContext(user_info);
  login_mixin_.WaitForActiveSession();

  EXPECT_TRUE(UserSessionManager::GetInstance()
                  ->token_handle_backfill_tried_for_testing());
  // Token should not be there as there are no real auth data.
  EXPECT_TRUE(TokenHandleUtil::ShouldObtainHandle(user_info.account_id));

  // Emulate successful token fetch.
  TokenHandleUtil::StoreTokenHandle(user_info.account_id, kTokenHandle);
  EXPECT_FALSE(TokenHandleUtil::ShouldObtainHandle(user_info.account_id));
}

IN_PROC_BROWSER_TEST_F(TokenAfterCrash, ValidToken) {
  auto user_info = login_mixin_.users()[0];
  EXPECT_FALSE(UserSessionManager::GetInstance()
                   ->token_handle_backfill_tried_for_testing());
}

class IgnoreOldTokenTest
    : public LoginManagerTest,
      public LocalStateMixin::Delegate,
      public ::testing::WithParamInterface<bool> /* isManagedUser */ {
 public:
  IgnoreOldTokenTest() {
    if (IsManagedUser())
      login_mixin_.AppendManagedUsers(1);
    else
      login_mixin_.AppendRegularUsers(1);

    account_id_ = login_mixin_.users()[0].account_id;
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    TokenHandleUtil::StoreTokenHandle(account_id_, kTokenHandle);

    if (content::IsPreTest()) {
      // Keep `TokenHandleRotated` flag to disable logic of neglecting not
      // rotated token.
      return;
    }
  }

  // LoginManagerTest:
  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    TokenHandleUtil::SetInvalidTokenForTesting(kTokenHandle);
  }

  void TearDownInProcessBrowserTestFixture() override {
    TokenHandleUtil::SetInvalidTokenForTesting(nullptr);
    LoginManagerTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  bool IsManagedUser() const { return GetParam(); }

  LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id_;

  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

// Verify case when a user got token invalidated on a pre-rotated version and
// then never re-authenticated. Such scenario should now lead to an online
// sign-in and fetching a new token handle.
IN_PROC_BROWSER_TEST_P(IgnoreOldTokenTest, PRE_IgnoreNotRotated) {
  ASSERT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id_));
}

// If any pre-rotated token handle is still left for either regular or managed
// user it will verified as invalid and lead to online re-authenication.
IN_PROC_BROWSER_TEST_P(IgnoreOldTokenTest, IgnoreNotRotated) {
  ASSERT_TRUE(TokenHandleUtil::HasToken(account_id_));
  ASSERT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id_));
}

INSTANTIATE_TEST_SUITE_P(All, IgnoreOldTokenTest, testing::Bool());

}  // namespace ash
