// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/user_flow.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

class BrowserLoginTest : public chromeos::LoginManagerTest {
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
  chromeos::StartupUtils::MarkOobeCompleted();
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
  EXPECT_TRUE(browser != NULL);
  EXPECT_TRUE(browser->window()->IsActive());

  gfx::NativeWindow window = browser->window()->GetNativeWindow();
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  views::FocusManager* focus_manager = widget->GetFocusManager();
  EXPECT_TRUE(focus_manager != NULL);

  const views::View* focused_view = focus_manager->GetFocusedView();
  EXPECT_TRUE(focused_view != NULL);
  EXPECT_EQ(VIEW_ID_OMNIBOX, focused_view->GetID());
}

IN_PROC_BROWSER_TEST_F(BrowserLoginTest,
                       PRE_VirtualKeyboardFeaturesEnabledByDefault) {
  RegisterUser(
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId));
  EXPECT_EQ(session_manager::SessionState::OOBE,
            session_manager::SessionManager::Get()->session_state());
  chromeos::StartupUtils::MarkOobeCompleted();
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
      ash::KeyboardController::Get()->GetKeyboardConfig();
  EXPECT_TRUE(config.auto_capitalize);
  EXPECT_TRUE(config.auto_complete);
  EXPECT_TRUE(config.auto_correct);
  EXPECT_TRUE(config.handwriting);
  EXPECT_TRUE(config.spell_check);
  EXPECT_TRUE(config.voice_input);
}

class OnboardingUserActivityTest : public LoginManagerTest {
 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
  FakeGaiaMixin gaia_mixin_{&mixin_host_, embedded_test_server()};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
  AccountId regular_user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
};

IN_PROC_BROWSER_TEST_F(OnboardingUserActivityTest, PRE_RegularUser) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(regular_user_);
  login_mixin_.LoginWithDefaultContext(test_user);
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();

  ash::test::UserSessionManagerTestApi test_api(
      ash::UserSessionManager::GetInstance());
  ASSERT_TRUE(test_api.get_onboarding_user_activity_counter());
}

IN_PROC_BROWSER_TEST_F(OnboardingUserActivityTest, RegularUser) {
  login_mixin_.LoginAsNewRegularUser();
  ash::LoginScreenTestApi::SubmitPassword(regular_user_, "password",
                                          /*check_if_submittable=*/false);
  login_mixin_.WaitForActiveSession();

  ash::test::UserSessionManagerTestApi test_api(
      ash::UserSessionManager::GetInstance());
  ASSERT_TRUE(test_api.get_onboarding_user_activity_counter());
}

// Verifies that counter is not started for child user.
IN_PROC_BROWSER_TEST_F(OnboardingUserActivityTest, ChildUser) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  login_mixin_.LoginAsNewChildUser();
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();

  ash::test::UserSessionManagerTestApi test_api(
      ash::UserSessionManager::GetInstance());
  ASSERT_FALSE(test_api.get_onboarding_user_activity_counter());
}

}  // namespace chromeos
