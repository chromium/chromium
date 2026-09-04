// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_login_pref_names.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/reauth_reason.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/stub_authenticator.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

constexpr char kUserEmail[] = "test-user@gmail.com";
constexpr GaiaId::Literal kGaiaID("111111");
constexpr char kProfileSigninNotificationId[] = "chrome://settings/signin/";
constexpr char kTokenHandle[] = "test_token_handle";
constexpr char kTestingFileName[] = "testing-file.txt";
constexpr char kTokenHandleLastCheckedPref[] = "TokenHandleLastChecked";
constexpr char kTokenHandlePref[] = "PasswordTokenHandle";
constexpr char kTokenHandleStatusPref[] = "TokenHandleStatus";
constexpr char kTokenHandleStatusStale[] = "stale";

constexpr char kTestTokenHandle[] = "test-token-handle";

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

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_RETURN, 0);

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

// Verifies that AutoWipe is triggered when the
// `kDeviceOnlinePasswordMismatchBehavior` pref is set to 1.
IN_PROC_BROWSER_TEST_F(PasswordChangeTest,
                       DeviceOnlinePasswordMismatchBehavior_AutoWipe) {
  CreateTestingFile();
  OpenGaiaDialog(test_account_id_);

  // Set the AutoWipe behavior in Local State.
  g_browser_process->local_state()->SetInteger(
      ash::prefs::kDeviceOnlinePasswordMismatchBehavior,
      static_cast<int>(DeviceOnlinePasswordMismatchBehavior::kAutoWipe));

  // Mark the test user as enterprise managed so the AutoWipe policy triggers.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetIsEnterpriseManaged(test_account_id_, true);

  // Skip post-login screens to reach ACTIVE session state immediately after
  // wipe.
  login_mixin_.SkipPostLoginScreens();

  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  // Wait for the cryptohome removal to be triggered asynchronously.
  {
    base::RunLoop run_loop;
    base::RepeatingTimer timer;
    timer.Start(
        FROM_HERE, base::Milliseconds(100),
        base::BindRepeating(
            [](base::RunLoop* run_loop) {
              if (FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kRemove>()) {
                run_loop->Quit();
              }
            },
            &run_loop));
    // Fail the test if wipe doesn't happen within 10 seconds.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));
    run_loop.Run();
  }

  // Verify that the cryptohome was actually removed.
  EXPECT_TRUE(FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kRemove>());
  EXPECT_FALSE(TestingFileExists());

  // Wait for active session to fully stabilize before concluding the test to
  // avoid dangling profile pointers during teardown.
  login_mixin_.WaitForActiveSession();
}

// Verifies that recovery/enter-old-password screen is shown when the
// `kDeviceOnlinePasswordMismatchBehavior` pref is set to 0 (default).
IN_PROC_BROWSER_TEST_F(PasswordChangeTest,
                       DeviceOnlinePasswordMismatchBehavior_Default) {
  CreateTestingFile();
  OpenGaiaDialog(test_account_id_);

  // Set the default behavior in Local State.
  g_browser_process->local_state()->SetInteger(
      ash::prefs::kDeviceOnlinePasswordMismatchBehavior,
      static_cast<int>(DeviceOnlinePasswordMismatchBehavior::kDefault));

  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  // Verify that we land on the Enter Old Password screen.
  test::CreateOldPasswordEnterPageWaiter()->Wait();

  // Verify that the cryptohome was NOT removed.
  EXPECT_FALSE(FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kRemove>());
  EXPECT_TRUE(TestingFileExists());
}

// Verifies that AutoWipe is NOT triggered when the user is a consumer,
// even if the `kDeviceOnlinePasswordMismatchBehavior` pref is set to 1.
IN_PROC_BROWSER_TEST_F(PasswordChangeTest,
                       DeviceOnlinePasswordMismatchBehavior_AutoWipe_Consumer) {
  CreateTestingFile();
  OpenGaiaDialog(test_account_id_);

  // Set the AutoWipe behavior in Local State.
  g_browser_process->local_state()->SetInteger(
      ash::prefs::kDeviceOnlinePasswordMismatchBehavior,
      static_cast<int>(DeviceOnlinePasswordMismatchBehavior::kAutoWipe));

  // Explicitly ensure the test user is marked as a consumer.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetIsEnterpriseManaged(test_account_id_, false);

  SetGaiaScreenCredentials(test_account_id_, test::kNewPassword);

  // Verify that we land on the Enter Old Password screen instead of wiping.
  test::CreateOldPasswordEnterPageWaiter()->Wait();

  // Verify that the cryptohome was NOT removed.
  EXPECT_FALSE(FakeUserDataAuthClient::Get()->WasCalled<AuthOp::kRemove>());
  EXPECT_TRUE(TestingFileExists());
}

class PasswordChangeTokenCheck : public PasswordChangeTest {
 public:
  PasswordChangeTokenCheck() {
    login_mixin_.AppendRegularUsers(1);
    user_with_invalid_token_ = login_mixin_.users().back().account_id;
    ignore_sync_errors_for_test_ =
        SigninErrorNotifier::IgnoreSyncErrorsForTesting();
    UserDataAuthClient::InitializeFake();
  }

 protected:
  // PasswordChangeTest:
  void SetUpOnMainThread() override {
    PasswordChangeTest::SetUpOnMainThread();
    token_handle_store_ = TokenHandleStoreFactory::Get()->GetTokenHandleStore();
    token_handle_store_->SetInvalidTokenForTesting(kTokenHandle);
  }

  void TearDownOnMainThread() override {
    token_handle_store_->SetInvalidTokenForTesting(nullptr);
    token_handle_store_ = nullptr;
    LoginManagerTest::TearDownOnMainThread();
  }

  const message_center::Notification* FindProfileSigninNotification() {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        CreateUserScopedNotificationId(kProfileSigninNotificationId,
                                       active_user->username_hash()));
  }

  AccountId user_with_invalid_token_;
  raw_ptr<TokenHandleStore> token_handle_store_;
  std::unique_ptr<base::AutoReset<bool>> ignore_sync_errors_for_test_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenPasswordChange) {
  token_handle_store_->StoreTokenHandle(user_with_invalid_token_, kTokenHandle);

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

class PasswordChangeTokenCheckStaleToken : public PasswordChangeTest {
 public:
  PasswordChangeTokenCheckStaleToken() {
    // Disable feature to trigger old code path.
    scoped_feature_list_.InitAndDisableFeature(features::kUseTokenHandleStore);
    login_mixin_.AppendRegularUsers(1);
    user_with_stale_token_ = login_mixin_.users().back().account_id;
    ignore_sync_errors_for_test_ =
        SigninErrorNotifier::IgnoreSyncErrorsForTesting();
    UserDataAuthClient::InitializeFake();
  }

 protected:
  // PasswordChangeTest:
  void SetUpOnMainThread() override {
    PasswordChangeTest::SetUpOnMainThread();
    user_manager::KnownUser known_user(g_browser_process->local_state());

    known_user.SetStringPref(user_with_stale_token_, kTokenHandlePref,
                             kTestTokenHandle);
    known_user.SetStringPref(user_with_stale_token_, kTokenHandleStatusPref,
                             kTokenHandleStatusStale);
  }

  void TearDownOnMainThread() override {
    LoginManagerTest::TearDownOnMainThread();
  }

  AccountId user_with_stale_token_;
  std::unique_ptr<base::AutoReset<bool>> ignore_sync_errors_for_test_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheckStaleToken,
                       LoginScreenPasswordChange) {
  EXPECT_FALSE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_stale_token_));
  // Focus triggers token check.
  LoginScreenTestApi::FocusUser(user_with_stale_token_);
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(user_with_stale_token_));

  OpenGaiaDialog(user_with_stale_token_);

  base::HistogramTester histogram_tester;

  SetGaiaScreenCredentials(user_with_stale_token_, test::kNewPassword);

  test::CreateOldPasswordEnterPageWaiter()->Wait();

  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::kInvalidTokenHandle, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenNoPasswordChange) {
  token_handle_store_->StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
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

// Tests token handle check on the session start.
IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, PRE_Session) {
  // Focus triggers token check. User does not have stored token, so online
  // login should not be forced.
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);
  ASSERT_FALSE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));

  // Store invalid token to trigger notification in the session.
  token_handle_store_->StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Make token not "checked recently".
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPath(user_with_invalid_token_, kTokenHandleLastCheckedPref,
                     base::TimeToValue(base::Time()));

  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());

  login_mixin_.WaitForActiveSession();

  const message_center::Notification* notification =
      FindProfileSigninNotification();
  ASSERT_TRUE(notification);

  // Click on notification should trigger Chrome restart.
  base::RunLoop exit_waiter;
  auto subscription =
      browser_shutdown::AddAppTerminatingCallback(exit_waiter.QuitClosure());

  message_center::MessageCenter::Get()->ClickOnNotification(notification->id());
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
  token_handle_store_->StoreTokenHandle(user_with_invalid_token_, kTokenHandle);

  // Focus triggers token check and opens online
  LoginScreenTestApi::FocusUser(user_with_invalid_token_);
  OpenGaiaDialog(user_with_invalid_token_);

  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());

  login_mixin_.WaitForActiveSession();

  ASSERT_FALSE(FindProfileSigninNotification());
}

class TokenAfterCrash : public MixinBasedInProcessBrowserTest {
 public:
  TokenAfterCrash() {
    login_mixin_.set_session_restore_enabled();
    login_mixin_.SetShouldObtainHandle(true);
    login_mixin_.AppendRegularUsers(1);
    UserDataAuthClient::InitializeFake();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    token_handle_store_ = TokenHandleStoreFactory::Get()->GetTokenHandleStore();
  }

  void TearDownOnMainThread() override {
    token_handle_store_ = nullptr;
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
  raw_ptr<TokenHandleStore> token_handle_store_;
};

// Test that token handle is downloaded on browser crash.
IN_PROC_BROWSER_TEST_F(TokenAfterCrash, PRE_NoToken) {
  auto user_info = login_mixin_.users()[0];
  login_mixin_.LoginWithDefaultContext(user_info);
  login_mixin_.WaitForActiveSession();

  EXPECT_TRUE(UserSessionManager::GetInstance()
                  ->token_handle_backfill_tried_for_testing());
  // Token should not be there as there are no real auth data.
  EXPECT_TRUE(token_handle_store_->ShouldObtainHandle(user_info.account_id));
}

IN_PROC_BROWSER_TEST_F(TokenAfterCrash, NoToken) {
  auto user_info = login_mixin_.users()[0];
  EXPECT_TRUE(UserSessionManager::GetInstance()
                  ->token_handle_backfill_tried_for_testing());
  // Token should not be there as there are no real auth data.
  EXPECT_TRUE(token_handle_store_->ShouldObtainHandle(user_info.account_id));
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
  EXPECT_TRUE(token_handle_store_->ShouldObtainHandle(user_info.account_id));

  // Emulate successful token fetch.
  token_handle_store_->StoreTokenHandle(user_info.account_id, kTokenHandle);
  EXPECT_FALSE(token_handle_store_->ShouldObtainHandle(user_info.account_id));
}

IN_PROC_BROWSER_TEST_F(TokenAfterCrash, ValidToken) {
  auto user_info = login_mixin_.users()[0];
  EXPECT_FALSE(UserSessionManager::GetInstance()
                   ->token_handle_backfill_tried_for_testing());
}

class IgnoreOldTokenTest
    : public LoginManagerTest,
      public ::testing::WithParamInterface<bool> /* isManagedUser */ {
 public:
  IgnoreOldTokenTest() {
    if (IsManagedUser())
      login_mixin_.AppendManagedUsers(1);
    else
      login_mixin_.AppendRegularUsers(1);

    account_id_ = login_mixin_.users()[0].account_id;
    UserDataAuthClient::InitializeFake();
  }

  void PreRunTestOnMainThread() override {
    // Token is used in some set up done in PreRunTestOnMainThread(),
    // so set it up earlier than the timing.
    token_handle_store_ = TokenHandleStoreFactory::Get()->GetTokenHandleStore();
    token_handle_store_->StoreTokenHandle(account_id_, kTokenHandle);
    token_handle_store_->SetInvalidTokenForTesting(kTokenHandle);

    LoginManagerTest::PreRunTestOnMainThread();
  }

  void TearDownOnMainThread() override {
    token_handle_store_->SetInvalidTokenForTesting(nullptr);
    token_handle_store_ = nullptr;
    LoginManagerTest::TearDownOnMainThread();
  }

 protected:
  bool IsManagedUser() const { return GetParam(); }

  LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId account_id_;

  raw_ptr<TokenHandleStore> token_handle_store_;
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
  ASSERT_TRUE(token_handle_store_->HasToken(account_id_));
  ASSERT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id_));
}

INSTANTIATE_TEST_SUITE_P(All, IgnoreOldTokenTest, testing::Bool());

}  // namespace ash
