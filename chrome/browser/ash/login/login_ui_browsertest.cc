// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class InterruptedAutoStartEnrollmentTest : public OobeBaseTest,
                                           public LocalStateMixin::Delegate {
 public:
  InterruptedAutoStartEnrollmentTest() = default;
  ~InterruptedAutoStartEnrollmentTest() override = default;

  void SetUpLocalState() override {
    StartupUtils::MarkOobeCompleted();
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(::prefs::kDeviceEnrollmentAutoStart, true);
    prefs->SetBoolean(::prefs::kDeviceEnrollmentCanExit, false);
  }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

// Tests that the default first screen is the welcome screen after OOBE
// when auto enrollment is enabled and device is not yet enrolled.
IN_PROC_BROWSER_TEST_F(InterruptedAutoStartEnrollmentTest, ShowsWelcome) {
  test::WaitForWelcomeScreen();
}

IN_PROC_BROWSER_TEST_F(OobeBaseTest, OobeNoExceptions) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());

  test::WaitForWelcomeScreen();

  // Test minimum screen size and global ResizeObservers
  display_manager.UpdateDisplay(std::string("900x600"));
  // Test portrait transition
  display_manager.UpdateDisplay(std::string("600x900"));
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .UpdateNaturalOrientation();

  OobeBaseTest::CheckJsExceptionErrors(0);
}

IN_PROC_BROWSER_TEST_F(OobeBaseTest, OobeCatchException) {
  OobeBaseTest::CheckJsExceptionErrors(0);
  test::OobeJS().ExecuteAsync("aelrt('misprint')");
  OobeBaseTest::CheckJsExceptionErrors(1);
  test::OobeJS().ExecuteAsync("consle.error('Some error')");
  OobeBaseTest::CheckJsExceptionErrors(2);
}

class LoginUITestBase : public LoginManagerTest {
 public:
  LoginUITestBase() : LoginManagerTest() {
    login_manager_mixin_.AppendRegularUsers(10);
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

class LoginUIEnrolledTest : public LoginUITestBase {
 public:
  LoginUIEnrolledTest() = default;
  ~LoginUIEnrolledTest() override = default;

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

class LoginUIConsumerTest : public LoginUITestBase {
 public:
  LoginUIConsumerTest() = default;
  ~LoginUIConsumerTest() override = default;

  void SetUpOnMainThread() override {
    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner, base::Value(owner_.account_id.GetUserEmail()));
    LoginUITestBase::SetUpOnMainThread();
  }

 protected:
  LoginManagerMixin::TestUserInfo owner_{login_manager_mixin_.users()[3]};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUIConsumerTest, LoginUIVisible) {
  const auto& test_users = login_manager_mixin_.users();
  const int users_count = test_users.size();
  EXPECT_EQ(users_count, LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = 0; i < users_count; ++i) {
    EXPECT_TRUE(LoginScreenTestApi::FocusUser(test_users[i].account_id));
  }

  for (int i = 0; i < users_count; ++i) {
    // Can't remove the owner.
    EXPECT_EQ(LoginScreenTestApi::RemoveUser(test_users[i].account_id),
              test_users[i].account_id != owner_.account_id);
  }

  EXPECT_EQ(1, LoginScreenTestApi::GetUsersCount());
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(owner_.account_id));
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginUIEnrolledTest, UserRemoval) {
  const auto& test_users = login_manager_mixin_.users();
  const int users_count = test_users.size();
  EXPECT_EQ(users_count, LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());

  // Remove the first user.
  EXPECT_TRUE(LoginScreenTestApi::RemoveUser(test_users[0].account_id));
  EXPECT_EQ(users_count - 1, LoginScreenTestApi::GetUsersCount());

  // Can not remove twice.
  EXPECT_FALSE(LoginScreenTestApi::RemoveUser(test_users[0].account_id));
  EXPECT_EQ(users_count - 1, LoginScreenTestApi::GetUsersCount());

  for (int i = 1; i < users_count; ++i) {
    EXPECT_TRUE(LoginScreenTestApi::RemoveUser(test_users[i].account_id));
    EXPECT_EQ(users_count - i - 1, LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(LoginUIEnrolledTest, UserReverseRemoval) {
  const auto& test_users = login_manager_mixin_.users();
  const int users_count = test_users.size();
  EXPECT_EQ(users_count, LoginScreenTestApi::GetUsersCount());
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());

  for (int i = users_count - 1; i >= 0; --i) {
    EXPECT_TRUE(LoginScreenTestApi::RemoveUser(test_users[i].account_id));
    EXPECT_EQ(i, LoginScreenTestApi::GetUsersCount());
  }

  // Gaia dialog should be shown again as there are no users anymore.
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

class DisplayPasswordButtonTest : public LoginManagerTest {
 public:
  DisplayPasswordButtonTest() : LoginManagerTest() {}

  void LoginAndLock(const LoginManagerMixin::TestUserInfo& test_user) {
    login_manager_mixin_.SkipPostLoginScreens();

    auto context = LoginManagerMixin::CreateDefaultUserContext(test_user);
    login_manager_mixin_.LoginAsNewRegularUser(context);
    login_manager_mixin_.WaitForActiveSession();

    ScreenLockerTester screen_locker_tester;
    screen_locker_tester.Lock();

    EXPECT_TRUE(LoginScreenTestApi::FocusUser(test_user.account_id));
  }

  void SetDisplayPasswordButtonEnabledLoginAndLock(
      bool display_password_button_enabled) {
    // Enables the login display password buttonn by user policy.
    {
      std::unique_ptr<ScopedUserPolicyUpdate> scoped_user_policy_update =
          user_policy_mixin_.RequestPolicyUpdate();
      scoped_user_policy_update->policy_payload()
          ->mutable_logindisplaypasswordbuttonenabled()
          ->set_value(display_password_button_enabled);
    }

    LoginAndLock(managed_user_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    // Login as a managed user would save force-online-signin to true and
    // invalidate the auth token into local state, which would prevent to focus
    // during the second part of the test which happens in the login screen.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSkipForceOnlineSignInForTesting);
  }

  void TearDownInProcessBrowserTestFixture() override {
    LoginManagerTest::TearDownInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSkipForceOnlineSignInForTesting);
  }

 protected:
  const LoginManagerMixin::TestUserInfo not_managed_user_{
      AccountId::FromUserEmailGaiaId("user@gmail.com", "1111")};
  const LoginManagerMixin::TestUserInfo managed_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "22222")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, managed_user_.account_id};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Check if the display password button is shown on the lock screen after having
// logged into a session and having locked the screen for an unmanaged user.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       PRE_DisplayPasswordButtonShownUnmanagedUser) {
  LoginAndLock(not_managed_user_);
  EXPECT_TRUE(LoginScreenTestApi::IsDisplayPasswordButtonShown(
      not_managed_user_.account_id));
}

// Check if the display password button is shown on the login screen for an
// unmanaged user.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       DisplayPasswordButtonShownUnmanagedUser) {
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(not_managed_user_.account_id));
  EXPECT_TRUE(LoginScreenTestApi::IsDisplayPasswordButtonShown(
      not_managed_user_.account_id));
}

// Check if the display password button is hidden on the lock screen after
// having logged into a session and having locked the screen for a managed user.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       PRE_DisplayPasswordButtonHiddenManagedUser) {
  SetDisplayPasswordButtonEnabledLoginAndLock(false);
  EXPECT_FALSE(LoginScreenTestApi::IsDisplayPasswordButtonShown(
      managed_user_.account_id));
}

// Check if the display password button is hidden on the login screen for a
// managed user.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       DisplayPasswordButtonHiddenManagedUser) {
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(managed_user_.account_id));
  EXPECT_FALSE(LoginScreenTestApi::IsDisplayPasswordButtonShown(
      managed_user_.account_id));
}

// Check if the display password button is shown on the lock screen after having
// logged into a session and having locked the screen for a managed user.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       PRE_DisplayPasswordButtonShownManagedUser) {
  SetDisplayPasswordButtonEnabledLoginAndLock(true);
  EXPECT_TRUE(LoginScreenTestApi::IsDisplayPasswordButtonShown(
      managed_user_.account_id));
}

// Check if the display password button is shown on the login screen for a
// managed user.
IN_PROC_BROWSER_TEST_F(DisplayPasswordButtonTest,
                       DisplayPasswordButtonShownManagedUser) {
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(managed_user_.account_id));
  EXPECT_TRUE(LoginScreenTestApi::IsDisplayPasswordButtonShown(
      managed_user_.account_id));
}

// Checks that system info is visible independent of the Oobe dialog state.
IN_PROC_BROWSER_TEST_F(LoginUITestBase, SystemInfoVisible) {
  // No dialog due to existing users.
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsSystemInfoShown());

  // Open Oobe dialog.
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());

  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsSystemInfoShown());
}

// Checks accelerator works for toggle system info
IN_PROC_BROWSER_TEST_F(LoginUITestBase, ToggleSystemInfo) {
  // System info is present in the beginning
  EXPECT_TRUE(LoginScreenTestApi::IsSystemInfoShown());

  // System info is hidden when press alt + v
  EXPECT_TRUE(LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)));
  EXPECT_FALSE(LoginScreenTestApi::IsSystemInfoShown());

  // System info is shown when press alt + v again
  EXPECT_TRUE(LoginScreenTestApi::PressAccelerator(
      ui::Accelerator(ui::VKEY_V, ui::EF_ALT_DOWN)));
  EXPECT_TRUE(LoginScreenTestApi::IsSystemInfoShown());
}

class UserManagementDisclosureTest : public LoginManagerTest {
 public:
  UserManagementDisclosureTest() : LoginManagerTest() {}

  void LoginAndLock(const LoginManagerMixin::TestUserInfo& test_user,
                    UserPolicyMixin* user_policy_mixin) {
    if (user_policy_mixin) {
      user_policy_mixin->RequestPolicyUpdate();
    }

    login_manager_mixin_.SkipPostLoginScreens();

    auto context = LoginManagerMixin::CreateDefaultUserContext(test_user);
    login_manager_mixin_.LoginAsNewRegularUser(context);
    login_manager_mixin_.WaitForActiveSession();

    ScreenLockerTester screen_locker_tester;
    screen_locker_tester.Lock();
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
    // Login as a managed user would save force-online-signin to true and
    // invalidate the auth token into local state, which would prevent to focus
    // during the second part of the test which happens in the login screen.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSkipForceOnlineSignInForTesting);
  }

  void TearDownInProcessBrowserTestFixture() override {
    LoginManagerTest::TearDownInProcessBrowserTestFixture();
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSkipForceOnlineSignInForTesting);
  }

 protected:
  const LoginManagerMixin::TestUserInfo not_managed_user{
      AccountId::FromUserEmailGaiaId("user@gmail.com", "1111")};
  const LoginManagerMixin::TestUserInfo managed_user{
      AccountId::FromUserEmailGaiaId("user@example.com", "11111")};
  UserPolicyMixin managed_user_policy_mixin_{&mixin_host_,
                                             managed_user.account_id};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Check if the user management disclosure is hidden on the lock screen after
// having logged an unmanaged user into a session and having locked the screen.
IN_PROC_BROWSER_TEST_F(UserManagementDisclosureTest,
                       PRE_EnterpriseIconInvisibleNotManagedUser) {
  LoginAndLock(not_managed_user, nullptr);
  EXPECT_FALSE(
      LoginScreenTestApi::IsManagedIconShown(not_managed_user.account_id));
  EXPECT_TRUE(
      LoginScreenTestApi::ShowRemoveAccountDialog(not_managed_user.account_id));
  EXPECT_FALSE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      not_managed_user.account_id));
}

// Check if the user management disclosure is shown on the login screen for an
// unmanaged user.
IN_PROC_BROWSER_TEST_F(UserManagementDisclosureTest,
                       EnterpriseIconInvisibleNotManagedUser) {
  EXPECT_FALSE(
      LoginScreenTestApi::IsManagedIconShown(not_managed_user.account_id));
  EXPECT_TRUE(
      LoginScreenTestApi::ShowRemoveAccountDialog(not_managed_user.account_id));
  EXPECT_FALSE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      not_managed_user.account_id));
}

// Check if the user management disclosure is shown on the lock screen after
// having logged a managed user into a session and having locked the screen.
IN_PROC_BROWSER_TEST_F(UserManagementDisclosureTest,
                       PRE_EnterpriseIconVisibleManagedUser) {
  LoginAndLock(managed_user, &managed_user_policy_mixin_);
  EXPECT_TRUE(LoginScreenTestApi::IsManagedIconShown(managed_user.account_id));
  EXPECT_TRUE(
      LoginScreenTestApi::ShowRemoveAccountDialog(managed_user.account_id));
  EXPECT_TRUE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      managed_user.account_id));
}

// Check if the user management disclosure is shown on the login screen for a
// managed user.
IN_PROC_BROWSER_TEST_F(UserManagementDisclosureTest,
                       EnterpriseIconVisibleManagedUser) {
  EXPECT_TRUE(
      LoginScreenTestApi::ShowRemoveAccountDialog(managed_user.account_id));
  EXPECT_TRUE(LoginScreenTestApi::IsManagedIconShown(managed_user.account_id));
  EXPECT_TRUE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      managed_user.account_id));
}

class UserManagementDisclosureChildTest
    : public MixinBasedInProcessBrowserTest {
 public:
  ~UserManagementDisclosureChildTest() override = default;

 protected:
  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                          embedded_test_server(),
                                          LoggedInUserMixin::LogInType::kChild};
};

// Check if the user management disclosure is hidden on the lock screen after
// having logged a child account into a session and having locked the screen.
IN_PROC_BROWSER_TEST_F(UserManagementDisclosureChildTest,
                       PRE_EnterpriseIconVisibleChildUser) {
  logged_in_user_mixin_.LogInUser(
      {ash::LoggedInUserMixin::LoginDetails::kNoBrowserLaunch});
  ScreenLockerTester screen_locker_tester;
  screen_locker_tester.Lock();
  EXPECT_FALSE(LoginScreenTestApi::IsManagedIconShown(
      logged_in_user_mixin_.GetAccountId()));
  EXPECT_TRUE(LoginScreenTestApi::ShowRemoveAccountDialog(
      logged_in_user_mixin_.GetAccountId()));
  EXPECT_FALSE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      logged_in_user_mixin_.GetAccountId()));
}

// Check if the user management disclosure is shown on the login screen for a
// child account.
IN_PROC_BROWSER_TEST_F(UserManagementDisclosureChildTest,
                       EnterpriseIconVisibleChildUser) {
  EXPECT_FALSE(LoginScreenTestApi::IsManagedIconShown(
      logged_in_user_mixin_.GetAccountId()));
  EXPECT_TRUE(LoginScreenTestApi::ShowRemoveAccountDialog(
      logged_in_user_mixin_.GetAccountId()));
  EXPECT_FALSE(LoginScreenTestApi::IsManagedMessageInDialogShown(
      logged_in_user_mixin_.GetAccountId()));
}

class LoginUIDiagnosticsTest : public LoginUITestBase {
 public:
  LoginUIDiagnosticsTest() = default;
  ~LoginUIDiagnosticsTest() override = default;

  static bool IsDiagnosticsDialogVisible() {
    return SystemWebDialogDelegate::HasInstance(GURL("chrome://diagnostics"));
  }

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Checks accelerator can launch diagnostics app
IN_PROC_BROWSER_TEST_F(LoginUIDiagnosticsTest, LaunchDiagnostics) {
  EXPECT_FALSE(IsDiagnosticsDialogVisible());

  EXPECT_TRUE(LoginScreenTestApi::PressAccelerator(ui::Accelerator(
      ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)));
  test::TestPredicateWaiter(
      base::BindRepeating(&LoginUIDiagnosticsTest::IsDiagnosticsDialogVisible))
      .Wait();

  EXPECT_TRUE(IsDiagnosticsDialogVisible());
}

class LoginUIDiagnosticsDisabledTest : public LoginUIDiagnosticsTest {
 public:
  LoginUIDiagnosticsDisabledTest() = default;
  ~LoginUIDiagnosticsDisabledTest() override = default;

  bool IsDiagnosticsDialogVisible() {
    return SystemWebDialogDelegate::HasInstance(GURL("chrome://diagnostics"));
  }

 protected:
  // LoginUiDiagnosticsTest:
  void SetUpInProcessBrowserTestFixture() override {
    LoginUIDiagnosticsTest::SetUpInProcessBrowserTestFixture();
    // Disable the device
    std::unique_ptr<ScopedDevicePolicyUpdate> policy_update =
        device_state_.RequestDevicePolicyUpdate();
    policy_update->policy_data()->mutable_device_state()->set_device_mode(
        enterprise_management::DeviceState::DEVICE_MODE_DISABLED);
  }
};

// Checks accelerator doesn't launch diagnostics app when device is disabled
IN_PROC_BROWSER_TEST_F(LoginUIDiagnosticsDisabledTest,
                       DoesNotLaunchDiagnostics) {
  EXPECT_FALSE(IsDiagnosticsDialogVisible());

  int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  EXPECT_TRUE(LoginScreenTestApi::PressAccelerator(ui::Accelerator(
      ui::VKEY_ESCAPE, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)));
  LoginScreenTestApi::WaitForUiUpdate(ui_update_count);

  EXPECT_FALSE(IsDiagnosticsDialogVisible());
}

class SshWarningTest : public OobeBaseTest,
                       public ::testing::WithParamInterface<bool> {
 public:
  class TestDebugDaemonClient : public FakeDebugDaemonClient {
   public:
    void QueryDebuggingFeatures(
        DebugDaemonClient::QueryDevFeaturesCallback callback) override {
      std::move(callback).Run(/*succeeded=*/true, flags_);
    }

    void set_flags(int flags) { flags_ = flags; }

   private:
    int flags_ = debugd::DevFeatureFlag::DEV_FEATURES_DISABLED;
  };

  void SetUpOnMainThread() override {
    test_client_ = std::make_unique<TestDebugDaemonClient>();
    test_client_->set_flags(
        GetParam() ? debugd::DevFeatureFlag::DEV_FEATURE_SSH_SERVER_CONFIGURED
                   : debugd::DevFeatureFlag::DEV_FEATURES_DISABLED);
    DebugDaemonClient::SetInstanceForTest(test_client_.get());
    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    test_client_.reset();
    DebugDaemonClient::SetInstanceForTest(nullptr);
    OobeBaseTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<TestDebugDaemonClient> test_client_;
};

IN_PROC_BROWSER_TEST_P(SshWarningTest, VisibilityOnGaia) {
  WizardController::default_controller()->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::UIPath ssh_warning = {"gaia-signin", "signin-frame-dialog",
                              "sshWarning"};
  if (GetParam()) {
    test::OobeJS().ExpectVisiblePath(ssh_warning);
  } else {
    test::OobeJS().ExpectHiddenPath(ssh_warning);
  }
}

IN_PROC_BROWSER_TEST_P(SshWarningTest, VisibilityOnEnrollment) {
  WizardController::default_controller()->SkipToLoginForTesting();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kStartEnrollment);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  test::UIPath ssh_warning = {"enterprise-enrollment", "step-signin",
                              "sshWarning"};
  if (GetParam()) {
    test::OobeJS().ExpectVisiblePath(ssh_warning);
  } else {
    test::OobeJS().ExpectHiddenPath(ssh_warning);
  }
}

INSTANTIATE_TEST_SUITE_P(All, SshWarningTest, ::testing::Bool());

namespace {

// Names of policies.
constexpr char kManagedGuestModeName[] = "MANAGED_GUEST_MODE";
constexpr char kAllowNewUsersName[] = "ALLOW_NEW_USERS";

}  // namespace

class KioskSkuLoginScreenVisibilityTest
    : public MixinBasedInProcessBrowserTest {
 public:
  KioskSkuLoginScreenVisibilityTest() : MixinBasedInProcessBrowserTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~KioskSkuLoginScreenVisibilityTest() override = default;
  KioskSkuLoginScreenVisibilityTest(const KioskSkuLoginScreenVisibilityTest&) =
      delete;
  void operator=(const KioskSkuLoginScreenVisibilityTest&) = delete;

  void SetUpOnMainThread() override {
    Shell::Get()->login_screen_controller()->ShowLoginScreen();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitch(
        switches::kDisableOOBEChromeVoxHintTimerForTesting);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetKioskSku() {
    policy_helper()->device_policy()->policy_data().set_license_sku(
        policy::kKioskSkuName);
  }

  void AddKioskApp() {
    KioskAppsMixin::AppendKioskAccount(
        &policy_helper()->device_policy()->payload());
  }

 protected:
  policy::DevicePolicyCrosTestHelper* policy_helper() {
    return &policy_helper_;
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

// Verifies that shelf buttons of Guest mode and Add user are shown, and kiosk
// instruction bubble is hidden without kiosk SKU.
IN_PROC_BROWSER_TEST_F(KioskSkuLoginScreenVisibilityTest, WithoutKioskSku) {
  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAppsButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskDefaultMessageShown());
}

// Verifies that shelf buttons of Guest mode and Add user are hidden, kiosk
// instruction bubble is hidden, and kiosk
// default message is shown without kiosk apps.
IN_PROC_BROWSER_TEST_F(KioskSkuLoginScreenVisibilityTest, WithoutApps) {
  SetKioskSku();
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAppsButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_TRUE(LoginScreenTestApi::IsKioskDefaultMessageShown());
}

// Verifies that shelf buttons of Guest mode and Add user are hidden, kiosk
// instruction bubble is shown, and kiosk
// default message is hidden with kiosk apps.
IN_PROC_BROWSER_TEST_F(KioskSkuLoginScreenVisibilityTest, WithApps) {
  SetKioskSku();
  AddKioskApp();
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsAppsButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskDefaultMessageShown());
}

// Verifies that the class name of the "Apps" button is not changed. This is
// essential for the Kiosk `LaunchAppManually` TAST test.
IN_PROC_BROWSER_TEST_F(KioskSkuLoginScreenVisibilityTest,
                       ShouldNotChangeClassNameOfAppsButton) {
  SetKioskSku();
  AddKioskApp();
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_EQ(LoginScreenTestApi::GetAppsButtonClassName(), "KioskAppsButton");
}

// Verifies kiosk instruction bubble and kiosk
// default message are hidden when kiosk app menu is opened.
IN_PROC_BROWSER_TEST_F(KioskSkuLoginScreenVisibilityTest, OpenKioskMenu) {
  SetKioskSku();
  AddKioskApp();
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsAppsButtonShown());
  EXPECT_TRUE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskDefaultMessageShown());

  // Click app menu button
  EXPECT_TRUE(LoginScreenTestApi::ClickAppsButton());

  // Check visibility
  EXPECT_TRUE(LoginScreenTestApi::IsAppsButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskDefaultMessageShown());
}

// Verifies that kiosk default message is show even after ESC key is pressed.
IN_PROC_BROWSER_TEST_F(KioskSkuLoginScreenVisibilityTest,
                       TryDismissDefaultMessage) {
  SetKioskSku();
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_TRUE(LoginScreenTestApi::IsKioskDefaultMessageShown());

  // Focus the Kiosk default message
  EXPECT_TRUE(LoginScreenTestApi::FocusKioskDefaultMessage());

  // Press ESC key.
  ui::test::EventGenerator generator(Shell::Get()->GetPrimaryRootWindow());
  generator.PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  EXPECT_TRUE(LoginScreenTestApi::IsKioskDefaultMessageShown());
}

class KioskSkuLoginScreenPolicyTest
    : public KioskSkuLoginScreenVisibilityTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  void EnablePolicy() {
    std::string policy_name = GetParam();
    if (policy_name == kManagedGuestModeName) {
      policy_helper()
          ->device_policy()
          ->payload()
          .mutable_device_restricted_managed_guest_session_enabled()
          ->set_enabled(true);
    } else if (policy_name == kAllowNewUsersName) {
      policy_helper()
          ->device_policy()
          ->payload()
          .mutable_allow_new_users()
          ->set_allow_new_users(true);
    }
  }
};

IN_PROC_BROWSER_TEST_P(KioskSkuLoginScreenPolicyTest, EnabledPolicies) {
  SetKioskSku();
  EnablePolicy();
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();

  EXPECT_TRUE(LoginScreenTestApi::IsLoginShelfShown());
  EXPECT_TRUE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsKioskInstructionBubbleShown());
  EXPECT_TRUE(LoginScreenTestApi::IsKioskDefaultMessageShown());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskSkuLoginScreenPolicyTest,
                         testing::Values(kManagedGuestModeName,
                                         kAllowNewUsersName));

}  // namespace ash
