// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/offline_login_test_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kUser1Email[] = "test-user1@gmail.com";
constexpr char kGaia1ID[] = "111111";

constexpr char kUser2Email[] = "test-user2@gmail.com";
constexpr char kGaia2ID[] = "222222";

constexpr char kUser3Email[] = "test-user3@gmail.com";
constexpr char kGaia3ID[] = "333333";

constexpr base::TimeDelta kLoginOnlineShortDelay = base::Seconds(10);
constexpr base::TimeDelta kLoginOnlineLongDelay = base::Seconds(20);

const test::UIPath kErrorMessageGuestSigninLink = {"error-message",
                                                   "error-guest-signin-link"};
const test::UIPath kErrorMessageOfflineSigninLink = {
    "error-message", "error-offline-login-link"};

class UserSelectionScreenTest : public LoginManagerTest {
 public:
  UserSelectionScreenTest() {
    login_manager_mixin_.AppendRegularUsers(3);
    login_manager_mixin_.AppendManagedUsers(1);
  }
  UserSelectionScreenTest(const UserSelectionScreenTest&) = delete;
  UserSelectionScreenTest& operator=(const UserSelectionScreenTest&) = delete;
  ~UserSelectionScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Enable ARC. Otherwise, the banner would not show.
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Test that a banner shows up for known-unmanaged users that need dircrypto
// migration. Also test that no banner shows up for users that may be managed.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenTest, ShowDircryptoMigrationBanner) {
  const auto& users = login_manager_mixin_.users();
  // No banner for the first user since default is no migration.
  EXPECT_FALSE(LoginScreenTestApi::IsWarningBubbleShown());

  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  FakeUserDataAuthClient::TestApi::Get()->SetHomeEncryptionMethod(
      cryptohome::CreateAccountIdentifierFromAccountId(users[1].account_id),
      FakeUserDataAuthClient::HomeEncryptionMethod::kEcryptfs);

  // Focus the 2nd user pod (consumer).
  ASSERT_TRUE(LoginScreenTestApi::FocusUser(users[1].account_id));

  // Wait for FakeUserDataAuthClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should be shown for the 2nd user (consumer).
    return LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();
  histogram_tester->ExpectBucketCount("Ash.Login.Login.MigrationBanner", true,
                                      1);

  FakeUserDataAuthClient::TestApi::Get()->SetHomeEncryptionMethod(
      cryptohome::CreateAccountIdentifierFromAccountId(users[2].account_id),
      FakeUserDataAuthClient::HomeEncryptionMethod::kDirCrypto);
  histogram_tester = std::make_unique<base::HistogramTester>();
  // Focus the 3rd user pod (consumer).
  ASSERT_TRUE(LoginScreenTestApi::FocusUser(users[2].account_id));

  // Wait for FakeUserDataAuthClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should be shown for the 3rd user (consumer).
    return !LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();
  histogram_tester->ExpectBucketCount("Ash.Login.Login.MigrationBanner", false,
                                      1);

  FakeUserDataAuthClient::TestApi::Get()->SetHomeEncryptionMethod(
      cryptohome::CreateAccountIdentifierFromAccountId(users[3].account_id),
      FakeUserDataAuthClient::HomeEncryptionMethod::kEcryptfs);
  histogram_tester = std::make_unique<base::HistogramTester>();

  // Focus to the 4th user pod (enterprise).
  ASSERT_TRUE(LoginScreenTestApi::FocusUser(users[3].account_id));

  // Wait for FakeUserDataAuthClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should not be shown for the enterprise user.
    return !LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();

  // Not recorded for enterprise.
  histogram_tester->ExpectUniqueSample("Ash.Login.Login.MigrationBanner", false,
                                       0);
}

class UserSelectionScreenEnforceOnlineTest : public LoginManagerTest,
                                             public LocalStateMixin::Delegate {
 public:
  UserSelectionScreenEnforceOnlineTest() : LoginManagerTest() {
    login_manager_mixin_.AppendManagedUsers(2);
  }
  ~UserSelectionScreenEnforceOnlineTest() override = default;
  UserSelectionScreenEnforceOnlineTest(
      const UserSelectionScreenEnforceOnlineTest&) = delete;
  UserSelectionScreenEnforceOnlineTest& operator=(
      const UserSelectionScreenEnforceOnlineTest&) = delete;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    const auto& users = login_manager_mixin_.users();
    const base::Time now = base::DefaultClock::GetInstance()->Now();

    user_manager::KnownUser known_user(g_browser_process->local_state());
    // User with expired offline login timeout.
    known_user.SetLastOnlineSignin(users[0].account_id,
                                   now - kLoginOnlineLongDelay);
    known_user.SetOfflineSigninLimit(users[0].account_id,
                                     kLoginOnlineShortDelay);

    // User withoin offline login timeout.
    known_user.SetLastOnlineSignin(users[1].account_id, now);
    known_user.SetOfflineSigninLimit(users[1].account_id,
                                     kLoginOnlineShortDelay);
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(UserSelectionScreenEnforceOnlineTest,
                       IsOnlineLoginEnforced) {
  const auto& users = login_manager_mixin_.users();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(users[0].account_id));
  EXPECT_FALSE(LoginScreenTestApi::IsForcedOnlineSignin(users[1].account_id));
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(users[0].account_id));
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

class UserSelectionScreenBlockOfflineTest : public LoginManagerTest,
                                            public LocalStateMixin::Delegate {
 public:
  UserSelectionScreenBlockOfflineTest() = default;
  ~UserSelectionScreenBlockOfflineTest() override = default;
  UserSelectionScreenBlockOfflineTest(
      const UserSelectionScreenBlockOfflineTest&) = delete;
  UserSelectionScreenBlockOfflineTest& operator=(
      const UserSelectionScreenBlockOfflineTest&) = delete;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    const base::Time now = base::DefaultClock::GetInstance()->Now();

    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetLastOnlineSignin(test_user_over_the_limit_.account_id,
                                   now - kLoginOnlineLongDelay);
    known_user.SetOfflineSigninLimit(test_user_over_the_limit_.account_id,
                                     kLoginOnlineShortDelay);

    known_user.SetLastOnlineSignin(test_user_under_the_limit_.account_id, now);
    known_user.SetOfflineSigninLimit(test_user_under_the_limit_.account_id,
                                     kLoginOnlineShortDelay);
  }

 protected:
  void OpenGaiaDialog(const AccountId& account_id) {
    EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
    EXPECT_TRUE(LoginScreenTestApi::IsForcedOnlineSignin(account_id));
    EXPECT_TRUE(LoginScreenTestApi::FocusUser(account_id));
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  }

  const LoginManagerMixin::TestUserInfo test_user_over_the_limit_{
      AccountId::FromUserEmailGaiaId(kUser1Email, kGaia1ID),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup).RequireReauth()};
  const LoginManagerMixin::TestUserInfo test_user_under_the_limit_{
      AccountId::FromUserEmailGaiaId(kUser2Email, kGaia2ID),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup).RequireReauth()};
  const LoginManagerMixin::TestUserInfo test_user_limit_not_set_{
      AccountId::FromUserEmailGaiaId(kUser3Email, kGaia3ID),
      test::UserAuthConfig::Create(test::kDefaultAuthSetup).RequireReauth()};
  LoginManagerMixin login_mixin_{
      &mixin_host_,
      {test_user_over_the_limit_, test_user_under_the_limit_,
       test_user_limit_not_set_}};
  OfflineLoginTestMixin offline_login_test_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

// Tests that offline login link is hidden on the network error screen when
// offline login period expires.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenBlockOfflineTest, HideOfflineLink) {
  offline_login_test_mixin_.GoOffline();
  OpenGaiaDialog(test_user_over_the_limit_.account_id);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kErrorMessageGuestSigninLink);
  test::OobeJS().ExpectHiddenPath(kErrorMessageOfflineSigninLink);
}

// Validates that offline login link is visible on the network error screen.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenBlockOfflineTest, ShowOfflineLink) {
  offline_login_test_mixin_.GoOffline();
  OpenGaiaDialog(test_user_under_the_limit_.account_id);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kErrorMessageGuestSigninLink);
  test::OobeJS().ExpectVisiblePath(kErrorMessageOfflineSigninLink);
}

// Offline login link is always shown when offline login time limit policy is
// not set.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenBlockOfflineTest,
                       OfflineLimitNotSet) {
  offline_login_test_mixin_.GoOffline();
  OpenGaiaDialog(test_user_limit_not_set_.account_id);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kErrorMessageGuestSigninLink);
  test::OobeJS().ExpectVisiblePath(kErrorMessageOfflineSigninLink);
}

class DarkLightEnabledTest : public LoginManagerTest {
 protected:
  void StartLogin(const AccountId& account_id) {
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->defer_oobe_flow_finished_for_tests = true;
    UserContext user_context = LoginManagerMixin::CreateDefaultUserContext(
        LoginManagerMixin::TestUserInfo(account_id));
    login_manager_mixin_.LoginAsNewRegularUser(user_context);
  }
  void FinishLogin() {
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->defer_oobe_flow_finished_for_tests = false;
    login_manager_mixin_.SkipPostLoginScreens();
    login_manager_mixin_.WaitForActiveSession();
  }

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  const AccountId user1{AccountId::FromUserEmailGaiaId(kUser1Email, kGaia1ID)};
  const AccountId user2{AccountId::FromUserEmailGaiaId(kUser2Email, kGaia2ID)};
};

// OOBE + login of the first user.
IN_PROC_BROWSER_TEST_F(DarkLightEnabledTest, PRE_PRE_OobeLogin) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());

  StartLogin(user1);
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());
  auto* profile = ProfileManager::GetActiveUserProfile();
  profile->GetPrefs()->SetBoolean(prefs::kDarkModeEnabled, true);
  // Still not enabled because OOBE is shown.
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());

  FinishLogin();
  // Oobe is hidden - prefs are applied.
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());
}

// "Add person" flow.
IN_PROC_BROWSER_TEST_F(DarkLightEnabledTest, PRE_OobeLogin) {
  // Oobe is hidden - prefs of the focused user are applied.
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

  LoginScreenTestApi::ClickAddUserButton();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  // Oobe is shown - switch to the light mode.
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());

  // Close the dialog with the `cancel` accelerator.
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kCancelScreenAction);
  test::TestPredicateWaiter(base::BindRepeating([]() {
    return !LoginScreenTestApi::IsOobeDialogVisible();
  })).Wait();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  // Switch back.
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

  LoginScreenTestApi::ClickAddUserButton();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();

  StartLogin(user2);
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());
  FinishLogin();
  auto* profile = ProfileManager::GetActiveUserProfile();
  profile->GetPrefs()->SetBoolean(prefs::kDarkModeEnabled, true);
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

  profile->GetPrefs()->SetBoolean(prefs::kDarkModeEnabled, false);
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());
}

// Test focusing different pods.
IN_PROC_BROWSER_TEST_F(DarkLightEnabledTest, OobeLogin) {
  ASSERT_EQ(LoginScreenTestApi::GetFocusedUser(), user2);
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());

  ASSERT_TRUE(LoginScreenTestApi::FocusUser(user1));
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

  ASSERT_TRUE(LoginScreenTestApi::FocusUser(user2));
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());
}

}  // namespace
}  // namespace ash
