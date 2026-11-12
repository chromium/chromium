// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "ash/system/power/power_event_observer_test_api.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kOnboardingBackfillVersion[] = "0.0.0.0";

// Counts OnLocaleChanged() notifications dispatched to Ash UI components.
class TestLocaleChangeObserver : public LocaleChangeObserver {
 public:
  TestLocaleChangeObserver() {
    LocaleUpdateController::Get()->AddObserver(this);
  }
  ~TestLocaleChangeObserver() override {
    LocaleUpdateController::Get()->RemoveObserver(this);
  }

  void OnLocaleChanged() override { ++locale_changed_count_; }

  int locale_changed_count() const { return locale_changed_count_; }

 private:
  int locale_changed_count_ = 0;
};

}  // namespace

class BrowserLoginTest : public LoginManagerTest {
 public:
  BrowserLoginTest() { set_should_launch_browser(true); }

  ~BrowserLoginTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kCreateBrowserOnStartupForTests);
  }
};

IN_PROC_BROWSER_TEST_F(BrowserLoginTest, PRE_BrowserActive) {
  RegisterUser(AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                              GaiaId(test::kTestGaiaId)));
  EXPECT_EQ(session_manager::SessionState::OOBE,
            session_manager::SessionManager::Get()->session_state());
  StartupUtils::MarkOobeCompleted(
      CHECK_DEREF(g_browser_process->local_state()));
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest, BrowserActive) {
  base::HistogramTester histograms;
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  LoginUser(AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                           GaiaId(test::kTestGaiaId)));
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  histograms.ExpectTotalCount("OOBE.BootToSignInCompleted", 1);

  BrowserWindowInterface* browser =
      ui_test_utils::FindAnyBrowser(ProfileManager::GetActiveUserProfile(),
                                    /*match_original_profiles=*/false);
  EXPECT_TRUE(browser != nullptr);
  EXPECT_TRUE(browser->IsActive());

  gfx::NativeWindow window = browser->GetWindow()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  EXPECT_TRUE(focus_manager != nullptr);

  const views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_TRUE(focused_view != nullptr);
  EXPECT_EQ(VIEW_ID_OMNIBOX, focused_view->GetID());
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest,
                       PRE_VirtualKeyboardFeaturesEnabledByDefault) {
  RegisterUser(AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                              GaiaId(test::kTestGaiaId)));
  EXPECT_EQ(session_manager::SessionState::OOBE,
            session_manager::SessionManager::Get()->session_state());
  StartupUtils::MarkOobeCompleted(
      CHECK_DEREF(g_browser_process->local_state()));
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest,
                       VirtualKeyboardFeaturesEnabledByDefault) {
  base::HistogramTester histograms;
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  LoginUser(AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                           GaiaId(test::kTestGaiaId)));
  EXPECT_TRUE(
      user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount());

  keyboard::KeyboardConfig config =
      KeyboardController::Get()->GetKeyboardConfig();
  EXPECT_TRUE(config.auto_capitalize);
  EXPECT_TRUE(config.auto_complete);
  EXPECT_TRUE(config.auto_correct);
  EXPECT_TRUE(config.handwriting);
  EXPECT_TRUE(config.spell_check);
  EXPECT_TRUE(config.voice_input);
}

// Sets up a login screen in "en-US" and a user whose profile locale is also
// "en-US".
IN_PROC_BROWSER_TEST_F(BrowserLoginTest, PRE_LocaleNotUpdatedWhenUnchanged) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      test::kTestEmail, GaiaId(test::kTestGaiaId));
  RegisterUser(account_id);
  StartupUtils::MarkOobeCompleted(
      CHECK_DEREF(g_browser_process->local_state()));
  LoginUser(account_id);

  // LoginManagerTest skips Gaia AccountInfoFetcher and Sync, which normally
  // populate kApplicationLocale on a real device, so set it explicitly. Also
  // mark it as accepted to suppress the locale-change confirmation dialog.
  PrefService* prefs =
      CHECK_DEREF(
          user_manager::UserManager::Get()->FindUserAndModify(account_id))
          .GetProfilePrefs();
  ASSERT_TRUE(prefs);
  prefs->SetString(language::prefs::kApplicationLocale, "en-US");
  prefs->SetString(ash::prefs::kApplicationLocaleAccepted, "en-US");
}

// Verifies that when a returning user logs in with the same UI locale as the
// login screen ("en-US" -> "en-US"), Ash is not notified to reload UI strings.
IN_PROC_BROWSER_TEST_F(BrowserLoginTest, LocaleNotUpdatedWhenUnchanged) {
  // Login screen starts in "en-US".
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());

  TestLocaleChangeObserver observer;
  LoginUser(AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                           GaiaId(test::kTestGaiaId)));
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_EQ(0, observer.locale_changed_count());
}

// Sets up a login screen in "en-US" and a user whose UI locale is "fr".
IN_PROC_BROWSER_TEST_F(BrowserLoginTest, PRE_LocaleUpdatedWhenChanged) {
  const AccountId account_id = AccountId::FromUserEmailGaiaId(
      test::kTestEmail, GaiaId(test::kTestGaiaId));
  RegisterUser(account_id);
  StartupUtils::MarkOobeCompleted(
      CHECK_DEREF(g_browser_process->local_state()));
  LoginUser(account_id);

  // Write the profile prefs directly instead of calling ChangeAppLocale(),
  // which would also update kOwnerLocale in local_state and switch the login
  // screen to "fr" on the next boot.
  PrefService* prefs =
      CHECK_DEREF(
          user_manager::UserManager::Get()->FindUserAndModify(account_id))
          .GetProfilePrefs();
  ASSERT_TRUE(prefs);
  prefs->SetString(language::prefs::kApplicationLocale, "fr");
  prefs->SetString(ash::prefs::kApplicationLocaleAccepted, "fr");
}

// Verifies that when the user's UI locale ("fr") differs from the login
// screen's locale ("en-US"), logging in switches the UI locale to "fr" and
// notifies Ash once to reload UI strings.
IN_PROC_BROWSER_TEST_F(BrowserLoginTest, LocaleUpdatedWhenChanged) {
  // Login screen starts in "en-US".
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());

  TestLocaleChangeObserver observer;
  LoginUser(AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                           GaiaId(test::kTestGaiaId)));
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_EQ(1, observer.locale_changed_count());
}

class OnboardingTest : public LoginManagerTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    cryptohome_mixin_.ApplyAuthConfigIfUserExists(
        regular_user_, test::UserAuthConfig::Create(test::kDefaultAuthSetup));
  }

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
  AccountId regular_user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                     GaiaId(test::kTestGaiaId))};

  EmbeddedPolicyTestServerMixin policy_server_mixin_{&mixin_host_};

  UserPolicyMixin user_policy_mixin_{&mixin_host_, regular_user_,
                                     &policy_server_mixin_};
};

// Verifies that OnboardingCompletedVersion is stored for new users.
IN_PROC_BROWSER_TEST_F(OnboardingTest, OnboardingCompletedVersion) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  OobeScreenExitWaiter user_creation_exit_waiter(UserCreationView::kScreenId);
  login_mixin_.LoginAsNewRegularUser();
  user_creation_exit_waiter.Wait();
  login_mixin_.SkipPostLoginScreens();
  login_mixin_.WaitForActiveSession();

  const session_manager::Session* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  ASSERT_TRUE(active_session);
  AccountId account_id = active_session->account_id();
  EXPECT_EQ(user_manager::KnownUser(g_browser_process->local_state())
                .GetOnboardingCompletedVersion(account_id),
            version_info::GetVersion());
}

// Verifies that OnboardingCompletedVersion is backfilled.
IN_PROC_BROWSER_TEST_F(OnboardingTest, PRE_OnboardingCompletedVersionBackfill) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(regular_user_);
  OobeScreenExitWaiter user_creation_exit_waiter(UserCreationView::kScreenId);
  UserContext user_context =
      LoginManagerMixin::CreateDefaultUserContext(test_user);
  login_mixin_.LoginAsNewRegularUser(user_context);
  user_creation_exit_waiter.Wait();
  login_mixin_.SkipPostLoginScreens();
  login_mixin_.WaitForActiveSession();

  const session_manager::Session* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  ASSERT_TRUE(active_session);
  AccountId account_id = active_session->account_id();
  user_manager::KnownUser(g_browser_process->local_state())
      .RemoveOnboardingCompletedVersionForTests(account_id);
}

IN_PROC_BROWSER_TEST_F(OnboardingTest, OnboardingCompletedVersionBackfill) {
  LoginScreenTestApi::SubmitPassword(regular_user_, "password",
                                     /*check_if_submittable=*/false);
  login_mixin_.WaitForActiveSession();

  const session_manager::Session* active_session =
      session_manager::SessionManager::Get()->GetActiveSession();
  ASSERT_TRUE(active_session);
  AccountId account_id = active_session->account_id();
  EXPECT_EQ(user_manager::KnownUser(g_browser_process->local_state())
                .GetOnboardingCompletedVersion(account_id),
            base::Version(kOnboardingBackfillVersion));
}

class LockOnSuspendUsageTest : public LoginManagerTest {
 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

// Verifies that tracking of the lock-on-suspend feature usage is started after
// user login.
IN_PROC_BROWSER_TEST_F(LockOnSuspendUsageTest, RegularUser) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  login_mixin_.SkipPostLoginScreens();
  login_mixin_.LoginAsNewRegularUser();
  login_mixin_.WaitForActiveSession();

  PowerEventObserverTestApi test_api(Shell::Get()->power_event_observer());
  ASSERT_TRUE(test_api.TrackingLockOnSuspendUsage());
}

}  // namespace ash
