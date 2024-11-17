// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "ash/system/power/power_event_observer_test_api.h"
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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kOnboardingBackfillVersion[] = "0.0.0.0";

}

class BrowserLoginTest : public LoginManagerTest {
 public:
  BrowserLoginTest() { set_should_launch_browser(true); }

  ~BrowserLoginTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kCreateBrowserOnStartupForTests);
  }
};

IN_PROC_BROWSER_TEST_F(BrowserLoginTest, PRE_BrowserActive) {
  RegisterUser(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId));
  EXPECT_EQ(session_manager::SessionState::OOBE,
            session_manager::SessionManager::Get()->session_state());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest, BrowserActive) {
  base::HistogramTester histograms;
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  LoginUser(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId));
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  histograms.ExpectTotalCount("OOBE.BootToSignInCompleted", 1);

  Browser* browser =
      chrome::FindAnyBrowser(ProfileManager::GetActiveUserProfile(), false);
  EXPECT_TRUE(browser != nullptr);
  EXPECT_TRUE(browser->window()->IsActive());

  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  EXPECT_TRUE(focus_manager != nullptr);

  const views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_TRUE(focused_view != nullptr);
  EXPECT_EQ(VIEW_ID_OMNIBOX, focused_view->GetID());
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest,
                       PRE_VirtualKeyboardFeaturesEnabledByDefault) {
  RegisterUser(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId));
  EXPECT_EQ(session_manager::SessionState::OOBE,
            session_manager::SessionManager::Get()->session_state());
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest,
                       VirtualKeyboardFeaturesEnabledByDefault) {
  base::HistogramTester histograms;
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  LoginUser(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId));
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
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};

  EmbeddedPolicyTestServerMixin policy_server_mixin_{&mixin_host_};

  UserPolicyMixin user_policy_mixin_{&mixin_host_, regular_user_,
                                     &policy_server_mixin_};
};

IN_PROC_BROWSER_TEST_F(OnboardingTest, PRE_OnboardingUserActivityRegularUser) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(regular_user_);
  login_mixin_.LoginWithDefaultContext(test_user);
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();

  test::UserSessionManagerTestApi test_api(UserSessionManager::GetInstance());
  ASSERT_TRUE(test_api.get_onboarding_user_activity_counter());
  login_mixin_.SkipPostLoginScreens();
}

// TODO(crbug.com/339860384): Enable the test.
IN_PROC_BROWSER_TEST_F(OnboardingTest,
                       DISABLED_OnboardingUserActivityRegularUser) {
  login_mixin_.LoginAsNewRegularUser();
  login_mixin_.WaitForActiveSession();

  test::UserSessionManagerTestApi test_api(UserSessionManager::GetInstance());
  ASSERT_TRUE(test_api.get_onboarding_user_activity_counter());
}

// Verifies that counter is not started for child user.
IN_PROC_BROWSER_TEST_F(OnboardingTest, OnboardingUserActivityChildUser) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  login_mixin_.LoginAsNewChildUser();
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();

  test::UserSessionManagerTestApi test_api(UserSessionManager::GetInstance());
  ASSERT_FALSE(test_api.get_onboarding_user_activity_counter());
}

// Verifies that OnboardingCompletedVersion is stored for new users.
IN_PROC_BROWSER_TEST_F(OnboardingTest, OnboardingCompletedVersion) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  OobeScreenExitWaiter user_creation_exit_waiter(UserCreationView::kScreenId);
  login_mixin_.LoginAsNewRegularUser();
  user_creation_exit_waiter.Wait();
  login_mixin_.SkipPostLoginScreens();
  login_mixin_.WaitForActiveSession();

  AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
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

  AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
  user_manager::KnownUser(g_browser_process->local_state())
      .RemoveOnboardingCompletedVersionForTests(account_id);
}

IN_PROC_BROWSER_TEST_F(OnboardingTest, OnboardingCompletedVersionBackfill) {
  LoginScreenTestApi::SubmitPassword(regular_user_, "password",
                                     /*check_if_submittable=*/false);
  login_mixin_.WaitForActiveSession();

  AccountId account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
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
