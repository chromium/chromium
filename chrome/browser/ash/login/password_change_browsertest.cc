// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator.h"
#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "test-user@gmail.com";
constexpr char kGaiaID[] = "111111";
constexpr char kTokenHandle[] = "test_token_handle";
constexpr char kTestingFileName[] = "testing-file.txt";

const test::UIPath kPasswordStep = {"gaia-password-changed", "passwordStep"};
const test::UIPath kOldPasswordInput = {"gaia-password-changed",
                                        "oldPasswordInput"};
const test::UIPath kSendPasswordButton = {"gaia-password-changed", "next"};
const test::UIPath kForgotPassword = {"gaia-password-changed",
                                      "forgotPasswordLink"};
const test::UIPath kTryAgain = {"gaia-password-changed", "tryAgain"};
const test::UIPath kProceedAnyway = {"gaia-password-changed", "proceedAnyway"};
const test::UIPath kCancel = {"gaia-password-changed", "cancel"};

}  // namespace

class PasswordChangeTestBase : public LoginManagerTest {
 public:
  PasswordChangeTestBase() = default;
  ~PasswordChangeTestBase() override = default;

 protected:
  virtual UserContext GetTestUserContext() {
    return login_mixin_.CreateDefaultUserContext(test_user_info_);
  }

  void OpenGaiaDialog(const AccountId& account_id) {
    EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
    EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));
    EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  }

  void WaitForPasswordChangeScreen() {
    OobeScreenWaiter(GaiaPasswordChangedView::kScreenId).Wait();
    OobeWindowVisibilityWaiter(true).Wait();

    EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
    EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
    EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  }

 protected:
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID);
  const LoginManagerMixin::TestUserInfo test_user_info_{
      test_account_id_, user_manager::UserType::USER_TYPE_REGULAR,
      user_manager::User::OAuthTokenStatus::OAUTH2_TOKEN_STATUS_INVALID};
  LoginManagerMixin login_mixin_{&mixin_host_, {test_user_info_}};
};

// Test fixture that uses a stub authenticator.
//
// Prefer using `PasswordChangeTest` for new tests instead.
class PasswordChangeStubAuthTest : public PasswordChangeTestBase {
 protected:
  // Sets up UserSessionManager to use stub authenticator that reports a
  // password change, and attempts login.
  // Password changed OOBE dialog is expected to show up after calling this.
  void SetUpStubAuthenticatorAndAttemptLogin(const std::string& old_password) {
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
    UserContext user_context = GetTestUserContext();

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    authenticator_builder->SetUpPasswordChange(
        old_password,
        base::BindRepeating(
            &PasswordChangeStubAuthTest::HandleDataRecoveryStatusChange,
            base::Unretained(this)));
    login_mixin_.AttemptLoginUsingAuthenticator(
        user_context, std::move(authenticator_builder));
  }

  StubAuthenticator::DataRecoveryStatus data_recovery_status_ =
      StubAuthenticator::DataRecoveryStatus::kNone;

 private:
  void HandleDataRecoveryStatusChange(
      StubAuthenticator::DataRecoveryStatus status) {
    EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kNone,
              data_recovery_status_);
    data_recovery_status_ = status;
  }
};

// Test fixture that uses a fake UserDataAuth in order to simulate password
// change flows.
class PasswordChangeTest : public PasswordChangeTestBase {
 protected:
  PasswordChangeTest() = default;

  void SetUpOnMainThread() override {
    PasswordChangeTestBase::SetUpOnMainThread();
    // Make `FakeUserDataAuthClient` perform actual password checks when
    // handling authentication requests. This is necessary for triggering the
    // password change UI flow.
    FakeUserDataAuthClient::TestApi::Get()->set_enable_auth_check(true);
  }

  void AddFakeUser(const std::string& password) {
    cryptohome_.MarkUserAsExisting(test_account_id_);
    cryptohome_.AddGaiaPassword(test_account_id_, password);
    CreateTestingFile();
  }

  bool TestingFileExists() const {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(GetTestingFilePath());
  }

  void SetGaiaScreenCredentials(const AccountId& account_id,
                                const std::string& password) {
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(account_id.GetUserEmail(), password,
                                  FakeGaiaMixin::kEmptyUserServices);
  }

 private:
  base::FilePath GetTestingFilePath() const {
    auto account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(test_account_id_);
    absl::optional<base::FilePath> profile_dir =
        FakeUserDataAuthClient::TestApi::Get()->GetUserProfileDir(
            account_identifier);
    if (!profile_dir) {
      ADD_FAILURE() << "Failed to get user profile dir";
      return base::FilePath();
    }
    return profile_dir.value().AppendASCII(kTestingFileName);
  }

  void CreateTestingFile() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(GetTestingFilePath(), /*data=*/""));
  }

  CryptohomeMixin cryptohome_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, MigrateOldCryptohome) {
  AddFakeUser("old user password");
  OpenGaiaDialog(test_account_id_);

  base::HistogramTester histogram_tester;
  SetGaiaScreenCredentials(test_account_id_, "new user password");
  WaitForPasswordChangeScreen();
  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kOther, 1);

  test::OobeJS().CreateVisibilityWaiter(true, kPasswordStep)->Wait();

  // Fill out and submit the old password passed to the userdataauth.
  test::OobeJS().TypeIntoPath("old user password", kOldPasswordInput);
  test::OobeJS().ClickOnPath(kSendPasswordButton);

  // User session should start, and whole OOBE screen is expected to be hidden.
  OobeWindowVisibilityWaiter(false).Wait();

  login_mixin_.WaitForActiveSession();
  EXPECT_TRUE(TestingFileExists());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeStubAuthTest, SubmitOnEnterKeyPressed) {
  OpenGaiaDialog(test_account_id_);

  base::HistogramTester histogram_tester;
  SetUpStubAuthenticatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kOther, 1);

  test::OobeJS().CreateVisibilityWaiter(true, kPasswordStep)->Wait();

  // Fill out and submit the old password passed to the stub authenticator.
  test::OobeJS().TypeIntoPath("old user password", kOldPasswordInput);
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_RETURN, false /* control */, false /* shift */,
      false /* alt */, false /* command */));

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, RetryOnWrongPassword) {
  AddFakeUser("old user password");
  OpenGaiaDialog(test_account_id_);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  SetGaiaScreenCredentials(test_account_id_, "new password");

  WaitForPasswordChangeScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kPasswordStep)->Wait();

  // Fill out and submit the old password passed to the fake userdataauth.
  test::OobeJS().TypeIntoPath("incorrect old user password", kOldPasswordInput);
  test::OobeJS().ClickOnPath(kSendPasswordButton);
  // Expect the UI to report failure.
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kOldPasswordInput) + ".invalid")
      ->Wait();
  test::OobeJS().ExpectEnabledPath(kPasswordStep);

  // Submit the correct password.
  test::OobeJS().TypeIntoPath("old user password", kOldPasswordInput);
  test::OobeJS().ClickOnPath(kSendPasswordButton);

  // User session should start, and whole OOBE screen is expected to be hidden.
  OobeWindowVisibilityWaiter(false).Wait();
  login_mixin_.WaitForActiveSession();
  EXPECT_TRUE(TestingFileExists());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, SkipDataRecovery) {
  AddFakeUser("old user password");
  OpenGaiaDialog(test_account_id_);
  SetGaiaScreenCredentials(test_account_id_, "new password");
  WaitForPasswordChangeScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kPasswordStep)->Wait();

  // Click forgot password link.
  test::OobeJS().ClickOnPath(kForgotPassword);

  test::OobeJS().CreateDisplayedWaiter(false, kPasswordStep)->Wait();

  test::OobeJS().ExpectVisiblePath(kTryAgain);
  test::OobeJS().ExpectVisiblePath(kProceedAnyway);

  // Click "Proceed anyway".
  test::OobeJS().ClickOnPath(kProceedAnyway);

  // User session should start, and whole OOBE screen is expected to be hidden.
  OobeWindowVisibilityWaiter(false).Wait();

  login_mixin_.WaitForActiveSession();
  EXPECT_FALSE(TestingFileExists());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeStubAuthTest,
                       TryAgainAfterForgetLinkClick) {
  OpenGaiaDialog(test_account_id_);
  SetUpStubAuthenticatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  test::OobeJS().CreateDisplayedWaiter(true, kPasswordStep)->Wait();

  // Click forgot password link.
  test::OobeJS().ClickOnPath(kForgotPassword);

  test::OobeJS().CreateDisplayedWaiter(false, kPasswordStep)->Wait();

  test::OobeJS().ExpectVisiblePath(kTryAgain);
  test::OobeJS().ExpectVisiblePath(kProceedAnyway);

  // Go back to old password input by clicking Try Again.
  test::OobeJS().ClickOnPath(kTryAgain);

  test::OobeJS().CreateDisplayedWaiter(true, kPasswordStep)->Wait();

  // Enter and submit the correct password.
  test::OobeJS().TypeIntoPath("old user password", kOldPasswordInput);
  test::OobeJS().ClickOnPath(kSendPasswordButton);

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeStubAuthTest, ClosePasswordChangedDialog) {
  OpenGaiaDialog(test_account_id_);
  SetUpStubAuthenticatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kPasswordStep)->Wait();

  test::OobeJS().TypeIntoPath("old user password", kOldPasswordInput);
  // Click the close button.
  test::OobeJS().ClickOnPath(kCancel);

  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kNone,
            data_recovery_status_);

  ExistingUserController::current_controller()->Login(GetTestUserContext(),
                                                      SigninSpecifics());
  OobeWindowVisibilityWaiter(true).Wait();
  OobeScreenWaiter(GaiaPasswordChangedView::kScreenId).Wait();
}

class PasswordChangeTokenCheck : public PasswordChangeStubAuthTest {
 public:
  PasswordChangeTokenCheck() {
    login_mixin_.AppendRegularUsers(1);
    user_with_invalid_token_ = login_mixin_.users().back().account_id;
    ignore_sync_errors_for_test_ =
        SigninErrorNotifier::IgnoreSyncErrorsForTesting();
  }

 protected:
  // PasswordChangeStubAuthTest:
  void SetUpInProcessBrowserTestFixture() override {
    PasswordChangeStubAuthTest::SetUpInProcessBrowserTestFixture();
    TokenHandleUtil::SetInvalidTokenForTesting(kTokenHandle);
  }
  void TearDownInProcessBrowserTestFixture() override {
    TokenHandleUtil::SetInvalidTokenForTesting(nullptr);
    PasswordChangeStubAuthTest::TearDownInProcessBrowserTestFixture();
  }

  UserContext GetTestUserContext() override {
    return login_mixin_.CreateDefaultUserContext(
        LoginManagerMixin::TestUserInfo(
            user_with_invalid_token_,
            user_manager::UserType::USER_TYPE_REGULAR));
  }

  AccountId user_with_invalid_token_;
  std::unique_ptr<base::AutoReset<bool>> ignore_sync_errors_for_test_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenPasswordChange) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Focus triggers token check.
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);

  OpenGaiaDialog(user_with_invalid_token_);
  base::HistogramTester histogram_tester;
  SetUpStubAuthenticatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
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
                                        notifications[0].id(), absl::nullopt,
                                        absl::nullopt);
  exit_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, Session) {
  ASSERT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));
  OpenGaiaDialog(user_with_invalid_token_);

  base::HistogramTester histogram_tester;
  SetUpStubAuthenticatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kInvalidTokenHandle, 1);
}

// Notification should not be triggered because token was checked on the login
// screen - recently.
IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, TokenRecentlyChecked) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Focus triggers token check.
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
    login_mixin_.set_should_obtain_handles(true);
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

class RotationTokenTest : public LoginManagerTest {
 public:
  RotationTokenTest() {
    login_mixin_.AppendRegularUsers(1);
    account_id_ = login_mixin_.users()[0].account_id;
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id_;
};

// Test verifies one-time rotation for the token handle.
IN_PROC_BROWSER_TEST_F(RotationTokenTest, PRE_Rotated) {
  TokenHandleUtil::StoreTokenHandle(account_id_, kTokenHandle);

  user_manager::KnownUser known_user(g_browser_process->local_state());
  // Emulate state before rotation.
  known_user.RemovePref(account_id_, "TokenHandleRotated");

  // Focus should not trigger online login.
  LoginScreenTestApi::FocusUser(account_id_);
  ASSERT_FALSE(LoginScreenTestApi::IsForcedOnlineSignin(account_id_));

  // Should be considered for rotation.
  EXPECT_TRUE(TokenHandleUtil::ShouldObtainHandle(account_id_));

  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());
  login_mixin_.WaitForActiveSession();

  // Emulate obtaining token handle.
  TokenHandleUtil::StoreTokenHandle(account_id_, kTokenHandle);
}

IN_PROC_BROWSER_TEST_F(RotationTokenTest, Rotated) {
  // Token should not be considered for rotation..
  EXPECT_FALSE(TokenHandleUtil::ShouldObtainHandle(account_id_));
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

    user_manager::KnownUser known_user(g_browser_process->local_state());
    // Emulate token was not rotated.
    known_user.RemovePref(account_id_, "TokenHandleRotated");
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

// Verify case when a user got token invalidated on a previous version and then
// updated to the version when not rotated tokens are ignored for managed users.
IN_PROC_BROWSER_TEST_P(IgnoreOldTokenTest, PRE_IgnoreNotRotated) {
  ASSERT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id_));
}

// Old tokens should be ignored for managed users. Regular users should be
// forced to go through online signin.
IN_PROC_BROWSER_TEST_P(IgnoreOldTokenTest, IgnoreNotRotated) {
  ASSERT_NE(TokenHandleUtil::HasToken(account_id_), IsManagedUser());
  ASSERT_NE(LoginScreenTestApi::IsForcedOnlineSignin(account_id_),
            IsManagedUser());
}

INSTANTIATE_TEST_SUITE_P(All, IgnoreOldTokenTest, testing::Bool());

}  // namespace ash
