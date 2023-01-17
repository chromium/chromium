// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kThemeSelectionId[] = "theme-selection";
constexpr char kLightThemeButton[] = "lightThemeButton";
constexpr char kDarkThemeButton[] = "darkThemeButton";
constexpr char kAutoThemeButton[] = "autoThemeButton";
const test::UIPath kDarkThemeButtonPath = {kThemeSelectionId, kDarkThemeButton};
const test::UIPath kLightThemeButtonPath = {kThemeSelectionId,
                                            kLightThemeButton};
const test::UIPath kAutoThemeButtonPath = {kThemeSelectionId, kAutoThemeButton};
const test::UIPath kNextButtonPath = {kThemeSelectionId, "nextButton"};
const test::UIPath kScreenSubtitleClamshellPath = {
    kThemeSelectionId, "theme-selection-subtitle-clamshell"};
const test::UIPath kScreenSubtitleTabletPath = {
    kThemeSelectionId, "theme-selection-subtitle-tablet"};

}  // namespace

class ThemeSelectionScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<test::UIPath> {
 public:
  ThemeSelectionScreenTest() {
    feature_list_.InitWithFeatures({chromeos::features::kDarkLightMode}, {});
  }

  void SetUpOnMainThread() override {
    ThemeSelectionScreen* theme_selection_screen =
        WizardController::default_controller()
            ->GetScreen<ThemeSelectionScreen>();

    theme_selection_screen->set_exit_callback_for_testing(base::BindRepeating(
        &ThemeSelectionScreenTest::HandleScreenExit, base::Unretained(this)));
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowThemeSelectionScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        ThemeSelectionScreenView::kScreenId);
  }

  void WaitForScreenExit() {
    if (result_.has_value())
      return;
    base::RunLoop run_loop;
    quit_closure_ = base::BindOnce(run_loop.QuitClosure());
    run_loop.Run();
  }

  void setTabletMode(bool enabled) {
    TabletMode::Waiter waiter(enabled);
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enabled);
    waiter.Wait();
  }

  absl::optional<ThemeSelectionScreen::Result> result_;

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  void HandleScreenExit(ThemeSelectionScreen::Result result) {
    result_ = result;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(ThemeSelectionScreenTest, ProceedWithDefaultTheme) {
  ShowThemeSelectionScreen();
  Profile* profile = ProfileManager::GetActiveUserProfile();
  test::OobeJS().ClickOnPath(kNextButtonPath);
  // Verify that remaining nudge shown count is 0 after proceeding with the
  // default theme.
  EXPECT_EQ(0, profile->GetPrefs()->GetInteger(
                   prefs::kDarkLightModeNudgeLeftToShowCount));
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenTest, SelectTheme) {
  ShowThemeSelectionScreen();
  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Expect the default dark mode schedule type to be sunset-to-sunrise.
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);

  test::OobeJS().ExpectVisiblePath(GetParam());
  test::OobeJS().ClickOnPath(GetParam());

  auto selectedOption = GetParam().begin()[GetParam().size() - 1];
  if (selectedOption == kDarkThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), true);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_TRUE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kLightThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), false);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_FALSE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kAutoThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);
  }

  test::OobeJS().ClickOnPath(kNextButtonPath);
  // Verify that remaining nudge shown count is 0 after user selects the theme.
  EXPECT_EQ(0, profile->GetPrefs()->GetInteger(
                   prefs::kDarkLightModeNudgeLeftToShowCount));
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(ThemeSelectionScreenTest, ToggleTabletMode) {
  ShowThemeSelectionScreen();
  // by default clamshell mode is used
  test::OobeJS().ExpectVisiblePath(kScreenSubtitleClamshellPath);

  // switch to tablet mode
  setTabletMode(true);
  test::OobeJS().ExpectVisiblePath(kScreenSubtitleTabletPath);

  // and back to clamshell
  setTabletMode(false);
  test::OobeJS().ExpectVisiblePath(kScreenSubtitleClamshellPath);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ThemeSelectionScreenTest,
                         ::testing::Values(kDarkThemeButtonPath,
                                           kLightThemeButtonPath,
                                           kAutoThemeButtonPath));

class ThemeSelectionScreenResumeTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<test::UIPath> {
 protected:
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
  AccountId user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
};

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenResumeTest, PRE_ResumedScreen) {
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(user_);
  login_mixin_.LoginWithDefaultContext(test_user);
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();
  WizardController::default_controller()->AdvanceToScreen(
      ThemeSelectionScreenView::kScreenId);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);

  test::OobeJS().ExpectVisiblePath(GetParam());
  test::OobeJS().ClickOnPath(GetParam());

  auto selectedOption = GetParam().begin()[GetParam().size() - 1];
  if (selectedOption == kDarkThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), true);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_TRUE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kLightThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), false);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_FALSE(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kAutoThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);
  }

  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenResumeTest, ResumedScreen) {
  login_mixin_.LoginAsNewRegularUser();
  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHasAttribute("checked", GetParam());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ThemeSelectionScreenResumeTest,
                         ::testing::Values(kDarkThemeButtonPath,
                                           kLightThemeButtonPath,
                                           kAutoThemeButtonPath));

}  // namespace ash
