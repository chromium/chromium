// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/offline_login_test_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/test/user_adding_screen_utils.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

const char kDomainAllowlist[] = "*@example.com";
const test::UIPath kOfflineLoginBackButton = {"offline-login", "backButton"};
const test::UIPath kWarningBackButton = {"offline-login",
                                         "offlineWarningBackButton"};

constexpr base::TimeDelta kLoginOnlineShortDelay = base::Seconds(10);
constexpr base::TimeDelta kLoginOnlineLongDelay = base::Seconds(20);

class LoginUserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(switches::kLoginProfile, "hash");
  }
};

class LoginGuestTest : public MixinBasedInProcessBrowserTest {
 protected:
  GuestSessionMixin guest_session_{&mixin_host_};
};

class LoginCursorTest : public OobeBaseTest {
 public:
  LoginCursorTest() = default;
  ~LoginCursorTest() override = default;
};

using LoginSigninTest = LoginManagerTest;

class LoginOfflineTest : public LoginManagerTest {
 public:
  LoginOfflineTest() {
    login_manager_.AppendRegularUsers(1);
    test_account_id_ = login_manager_.users()[0].account_id;
  }
  ~LoginOfflineTest() override = default;

 protected:
  AccountId test_account_id_;
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {},
                                   nullptr,
                                   &cryptohome_mixin_};
  OfflineLoginTestMixin offline_login_test_mixin_{&mixin_host_};
  // We need Fake gaia to avoid network errors that can be caused by
  // attempts to load real GAIA.
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
};

class LoginOnlineCryptohomeError : public LoginManagerTest {
 public:
  LoginOnlineCryptohomeError() = default;

 protected:
  LoginManagerMixin::TestUserInfo reauth_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup).RequireReauth()};
  LoginManagerMixin login_manager_{&mixin_host_, {reauth_user_}};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(LoginOnlineCryptohomeError, FatalScreenShown) {
  const auto& account_id = reauth_user_.account_id;
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  FakeUserDataAuthClient::Get()->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kStartAuthSession,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL));

  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  OobeScreenWaiter(SignInFatalErrorView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  test::ClickSignInFatalScreenActionButton();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, FatalScreenShown) {
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  FakeUserDataAuthClient::Get()->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          user_data_auth::CRYPTOHOME_ERROR_TPM_UPDATE_REQUIRED));
  LoginScreenTestApi::SubmitPassword(test_account_id_, "password",
                                     /*check_if_submittable=*/false);
  OobeScreenWaiter(SignInFatalErrorView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, FatalScreenNotShown) {
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  FakeUserDataAuthClient::Get()->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kAuthenticateAuthFactor,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED));
  LoginScreenTestApi::SubmitPassword(test_account_id_, "password",
                                     /*check_if_submittable=*/false);
  // Inserted RunUntilIdle here to give maximum chances for the dialog to show
  // up in case of a bug.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
}

class LoginOfflineManagedTest : public LoginManagerTest {
 public:
  LoginOfflineManagedTest() {
    login_manager_.AppendManagedUsers(1);
    managed_user_id_ = login_manager_.users()[0].account_id;
  }

  ~LoginOfflineManagedTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void ConfigurePolicy(const std::string& autocomplete_domain) {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_login_screen_domain_auto_complete()
        ->set_login_screen_domain_auto_complete(autocomplete_domain);
    device_policy_update->policy_payload()
        ->mutable_show_user_names()
        ->set_show_user_names(false);
  }

  void SetDeviceAllowNewUsersPolicy(bool enabled) {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_allow_new_users()
        ->set_allow_new_users(enabled);
  }

  void AddUserToAllowlist(const std::string& user_id) {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_user_allowlist()
        ->add_user_allowlist(user_id);
  }

  void SetUserOnlineLoginState() {
    const base::Time now = base::Time::Now();

    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetLastOnlineSignin(managed_user_id_,
                                   now - kLoginOnlineLongDelay);
    known_user.SetOfflineSigninLimit(managed_user_id_, kLoginOnlineShortDelay);
  }

 protected:
  AccountId managed_user_id_;
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_{&mixin_host_};
  OfflineLoginTestMixin offline_login_test_mixin_{&mixin_host_};
  // We need Fake gaia to avoid network errors that can be caused by
  // attempts to load real GAIA.
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

// Used to make sure that the system tray is visible and within the screen
// bounds after login.
void TestSystemTrayIsVisible() {
  aura::Window* primary_win = Shell::GetPrimaryRootWindow();
  Shelf* shelf = Shelf::ForWindow(primary_win);
  TrayBackgroundView* tray =
      shelf->GetStatusAreaWidget()->unified_system_tray();
  SCOPED_TRACE(testing::Message()
               << "ShelfVisibilityState=" << shelf->GetVisibilityState()
               << " ShelfAutoHideBehavior="
               << static_cast<int>(shelf->auto_hide_behavior()));
  StatusAreaWidgetTestHelper::WaitForAnimationEnd(shelf->GetStatusAreaWidget());
  EXPECT_TRUE(tray->GetVisible());

  // Wait for the system tray be inside primary bounds.
  test::TestPredicateWaiter(
      base::BindRepeating(
          [](const aura::Window* primary_win, const TrayBackgroundView* tray) {
            if (primary_win->bounds().Contains(tray->GetBoundsInScreen()))
              return true;
            LOG(WARNING) << primary_win->bounds().ToString()
                         << " does not contain "
                         << tray->GetBoundsInScreen().ToString();
            return false;
          },
          primary_win, tray))
      .Wait();
}

}  // namespace

// After a chrome crash, the session manager will restart chrome with
// the -login-user flag indicating that the user is already logged in.
// This profile should NOT be an OTR profile.
IN_PROC_BROWSER_TEST_F(LoginUserTest, UserPassed) {
  Profile* profile = browser()->profile();
  std::string profile_base_name =
      BrowserContextHelper::GetUserBrowserContextDirName("hash");
  EXPECT_EQ(profile_base_name, profile->GetBaseName().value());
  EXPECT_FALSE(profile->IsOffTheRecord());

  TestSystemTrayIsVisible();
}

// After a guest login, we should get the OTR default profile.
IN_PROC_BROWSER_TEST_F(LoginGuestTest, GuestIsOTR) {
  Profile* profile = browser()->profile();
  EXPECT_TRUE(profile->IsOffTheRecord());
  // Ensure there's extension service for this profile.
  EXPECT_TRUE(extensions::ExtensionSystem::Get(profile)->extension_service());

  TestSystemTrayIsVisible();
}

// Verifies the cursor is hidden at startup on login screen.
IN_PROC_BROWSER_TEST_F(LoginCursorTest, CursorHidden) {
  test::WaitForWelcomeScreen();
  // Cursor should be hidden at startup
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorVisible());

  // Cursor should be shown after cursor is moved.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(gfx::Point()));
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorVisible());

  TestSystemTrayIsVisible();
}

// Verifies that the webui for login comes up successfully.
IN_PROC_BROWSER_TEST_F(LoginSigninTest, WebUIVisible) {
  LoginOrLockScreenVisibleWaiter().Wait();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, PRE_AuthOffline) {
  offline_login_test_mixin_.PrepareOfflineLogin();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineTest, AuthOffline) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kOffline);
  offline_login_test_mixin_.GoOffline();
  offline_login_test_mixin_.InitOfflineLogin(test_account_id_,
                                             LoginManagerTest::kPassword);
  offline_login_test_mixin_.CheckManagedStatus(false);
  offline_login_test_mixin_.SubmitLoginAuthOfflineForm(
      test_account_id_.GetUserEmail(), LoginManagerTest::kPassword,
      true /* wait for sign-in */);
  TestSystemTrayIsVisible();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, CorrectDomainCompletion) {
  std::string domain = gaia::ExtractDomainName(managed_user_id_.GetUserEmail());

  ConfigurePolicy(domain);

  std::string email = managed_user_id_.GetUserEmail();
  size_t separator_pos = email.find('@');
  ASSERT_TRUE(separator_pos != email.npos &&
              separator_pos < email.length() - 1);
  std::string prefix = email.substr(0, separator_pos);

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  offline_login_test_mixin_.GoOffline();
  offline_login_test_mixin_.InitOfflineLogin(managed_user_id_,
                                             LoginManagerTest::kPassword);

  offline_login_test_mixin_.CheckManagedStatus(true);

  offline_login_test_mixin_.SubmitLoginAuthOfflineForm(
      prefix, LoginManagerTest::kPassword, true /* wait for sign-in */);
  TestSystemTrayIsVisible();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, FullEmailDontMatchProvided) {
  ConfigurePolicy("another.domain");

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  offline_login_test_mixin_.GoOffline();
  offline_login_test_mixin_.InitOfflineLogin(managed_user_id_,
                                             LoginManagerTest::kPassword);

  offline_login_test_mixin_.SubmitLoginAuthOfflineForm(
      managed_user_id_.GetUserEmail(), LoginManagerTest::kPassword,
      true /* wait for sign-in */);
  TestSystemTrayIsVisible();
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, BackButtonTest) {
  std::string domain = gaia::ExtractDomainName(managed_user_id_.GetUserEmail());

  ConfigurePolicy(domain);

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  offline_login_test_mixin_.GoOffline();
  offline_login_test_mixin_.InitOfflineLogin(managed_user_id_,
                                             LoginManagerTest::kPassword);

  test::OobeJS().ClickOnPath(kOfflineLoginBackButton);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  // Send network state update once again.
  offline_login_test_mixin_.GoOffline();
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, LoginAllowlistedUser) {
  std::string domain = gaia::ExtractDomainName(managed_user_id_.GetUserEmail());
  ConfigurePolicy(domain);
  SetDeviceAllowNewUsersPolicy(false);
  AddUserToAllowlist(kDomainAllowlist);

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  offline_login_test_mixin_.GoOffline();
  offline_login_test_mixin_.InitOfflineLogin(managed_user_id_,
                                             LoginManagerTest::kPassword);

  offline_login_test_mixin_.SubmitLoginAuthOfflineForm(
      managed_user_id_.GetUserEmail(), LoginManagerTest::kPassword,
      true /* wait for sign-in */);
}

IN_PROC_BROWSER_TEST_F(LoginOfflineManagedTest, UserOfflineLoginBlocked) {
  std::string domain = gaia::ExtractDomainName(managed_user_id_.GetUserEmail());

  ConfigurePolicy(domain);
  SetUserOnlineLoginState();

  std::string email = managed_user_id_.GetUserEmail();
  std::vector<std::string> email_and_domain = base::SplitString(
      email, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(email_and_domain.size(), 2u);
  std::string prefix = *(email_and_domain.begin());

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  offline_login_test_mixin_.GoOffline();
  offline_login_test_mixin_.InitOfflineLogin(managed_user_id_,
                                             LoginManagerTest::kPassword);

  offline_login_test_mixin_.CheckManagedStatus(true);

  // Submits only email to verify that the correct domain is appended
  // and the last online login timestamp found.
  offline_login_test_mixin_.SubmitEmailAndBlockOfflineFlow(prefix);
  test::OobeJS().ClickOnPath(kWarningBackButton);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

class UserAddingScreenTrayTest : public LoginManagerTest {
 public:
  UserAddingScreenTrayTest() { login_mixin_.AppendRegularUsers(3); }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(UserAddingScreenTrayTest, TrayVisible) {
  LoginUser(login_mixin_.users()[0].account_id);
  test::ShowUserAddingScreen();
  TestSystemTrayIsVisible();
}

IN_PROC_BROWSER_TEST_F(LoginManagerTest, SafeBrowsingDisabledForSigninProfile) {
  ASSERT_FALSE(ProfileHelper::GetSigninProfile()->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingEnabled));
}

}  // namespace ash
