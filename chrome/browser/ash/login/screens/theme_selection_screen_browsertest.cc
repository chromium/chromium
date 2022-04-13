// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/style/color_provider.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/guest_tos_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
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
}  // namespace

class ThemeSelectionScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<test::UIPath> {
 public:
  ThemeSelectionScreenTest() {
    feature_list_.InitWithFeatures({features::kEnableOobeThemeSelection,
                                    chromeos::features::kDarkLightMode},
                                   {});
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
  test::OobeJS().ClickOnPath(kNextButtonPath);
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_P(ThemeSelectionScreenTest, SelectTheme) {
  ShowThemeSelectionScreen();
  Profile* profile = ProfileManager::GetActiveUserProfile();

  EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), false);
  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);

  test::OobeJS().ExpectVisiblePath(GetParam());
  test::OobeJS().ClickOnPath(GetParam());

  auto selectedOption = GetParam().begin()[GetParam().size() - 1];
  if (selectedOption == kDarkThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), true);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_TRUE(ash::ColorProvider::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kLightThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled), false);
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 0);
    EXPECT_FALSE(ash::ColorProvider::Get()->IsDarkModeEnabled());

  } else if (selectedOption == kAutoThemeButton) {
    EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType), 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ThemeSelectionScreenTest,
                         ::testing::Values(kDarkThemeButtonPath,
                                           kLightThemeButtonPath,
                                           kAutoThemeButtonPath));

}  // namespace ash
