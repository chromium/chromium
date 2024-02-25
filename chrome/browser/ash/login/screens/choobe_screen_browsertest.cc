// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::UnorderedElementsAreArray;

constexpr char kChoobeScreenId[] = "choobe";
constexpr char kCrButtonTheme[] = "cr-button-theme-selection";
constexpr char kCrButtonTouchpad[] = "cr-button-touchpad-scroll";
constexpr char kCrButtonDisplay[] = "cr-button-display-size";

const test::UIPath kNextButtonPath = {kChoobeScreenId, "nextButton"};
const test::UIPath kSkipButtonPath = {kChoobeScreenId, "skipButton"};
const test::UIPath kDialogPath = {kChoobeScreenId, "choobeDialog"};
const test::UIPath kThemeSelectionTilePath = {kChoobeScreenId, "screensList",
                                              kCrButtonTheme};
const test::UIPath kTouchpadScrollTilePath = {kChoobeScreenId, "screensList",
                                              kCrButtonTouchpad};
const test::UIPath kDisplaySizeTilePath = {kChoobeScreenId, "screensList",
                                           kCrButtonDisplay};

}  // namespace

class ChoobeScreenTest : public OobeBaseTest {
 public:
  ChoobeScreenTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kOobeChoobe, ash::features::kOobeTouchpadScroll,
         ash::features::kOobeDisplaySize},
        {});
  }

  void SetUpOnMainThread() override {
    ChoobeScreen* choobe_screen =
        WizardController::default_controller()->GetScreen<ChoobeScreen>();

    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    test::SetFakeTouchpadDevice();
    original_callback_ = choobe_screen->get_exit_callback_for_testing();
    choobe_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowChoobeScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        ChoobeScreenView::kScreenId);
  }

  ChoobeScreen::Result WaitForScreenExitResult() {
    ChoobeScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::TestFuture<ChoobeScreen::Result> screen_result_waiter_;
  ChoobeScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(ChoobeScreenTest, Next) {
  ShowChoobeScreen();

  // Check Screen is visible and Next Button disabled
  test::OobeJS().ExpectVisiblePath(kDialogPath);
  test::OobeJS().ExpectDisabledPath(kNextButtonPath);

  // Select any Screen
  test::OobeJS().ClickOnPath(kThemeSelectionTilePath);

  // Check Next Button not disabled
  test::OobeJS().ExpectEnabledPath(kNextButtonPath);
  test::OobeJS().ClickOnPath(kNextButtonPath);
  ChoobeScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, ChoobeScreen::Result::SELECTED);
}

IN_PROC_BROWSER_TEST_F(ChoobeScreenTest, Skip) {
  ShowChoobeScreen();

  test::OobeJS().ExpectVisiblePath(kDialogPath);
  test::OobeJS().ExpectDisabledPath(kNextButtonPath);
  test::OobeJS().ClickOnPath(kSkipButtonPath);
  ChoobeScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, ChoobeScreen::Result::SKIPPED);
}

class ChoobeScreenDisabledScreenTest : public ChoobeScreenTest {
 public:
  ChoobeScreenDisabledScreenTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {ash::features::kOobeChoobe, ash::features::kOobeTouchpadScroll},
        {ash::features::kOobeDisplaySize});
  }
};

IN_PROC_BROWSER_TEST_F(ChoobeScreenDisabledScreenTest, NotEnoughScreen) {
  ShowChoobeScreen();
  ChoobeScreen::Result result = WaitForScreenExitResult();

  EXPECT_EQ(result, ChoobeScreen::Result::NOT_APPLICABLE);
}

class ChoobeScreenTestWithParams
    : public ChoobeScreenTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ChoobeScreenTestWithParams() {
    std::tie(is_theme_selection_selected, is_touchpad_scroll_selected,
             is_display_size_selected) = GetParam();
  }

  ChoobeScreenTestWithParams(const ChoobeScreenTestWithParams&) = delete;
  ChoobeScreenTestWithParams& operator=(const ChoobeScreenTestWithParams&) =
      delete;

  ~ChoobeScreenTestWithParams() override = default;

 protected:
  bool is_theme_selection_selected;
  bool is_touchpad_scroll_selected;
  bool is_display_size_selected;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ChoobeScreenTestWithParams,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

IN_PROC_BROWSER_TEST_P(ChoobeScreenTestWithParams, SelectTiles) {
  ShowChoobeScreen();

  test::OobeJS().ExpectVisiblePath(kDialogPath);

  base::Value::List expected_selected_screens_ids_;

  // Select Screens
  if (is_theme_selection_selected) {
    test::OobeJS().TapOnPath(kThemeSelectionTilePath);
    expected_selected_screens_ids_.Append(
        ThemeSelectionScreenView::kScreenId.name);
  }

  if (is_touchpad_scroll_selected) {
    test::OobeJS().TapOnPath(kTouchpadScrollTilePath);
    expected_selected_screens_ids_.Append(
        TouchpadScrollScreenView::kScreenId.name);
  }

  if (is_display_size_selected) {
    test::OobeJS().TapOnPath(kDisplaySizeTilePath);
    expected_selected_screens_ids_.Append(
        DisplaySizeScreenView::kScreenId.name);
  }

  // one of the screen should be selected otherwise next button would be
  // disabled, and only skip button would be clickable
  if (is_theme_selection_selected || is_touchpad_scroll_selected ||
      is_display_size_selected) {
    test::OobeJS().TapOnPath(kNextButtonPath);
    ChoobeScreen::Result result = WaitForScreenExitResult();
    EXPECT_EQ(result, ChoobeScreen::Result::SELECTED);

    EXPECT_EQ(WizardController::default_controller()
                  ->choobe_flow_controller()
                  ->ShouldScreenBeSkipped(DisplaySizeScreenView::kScreenId),
              !is_display_size_selected);
    EXPECT_EQ(WizardController::default_controller()
                  ->choobe_flow_controller()
                  ->ShouldScreenBeSkipped(ThemeSelectionScreenView::kScreenId),
              !is_theme_selection_selected);
    EXPECT_EQ(WizardController::default_controller()
                  ->choobe_flow_controller()
                  ->ShouldScreenBeSkipped(TouchpadScrollScreenView::kScreenId),
              !is_touchpad_scroll_selected);
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    EXPECT_TRUE(prefs->HasPrefPath(prefs::kChoobeSelectedScreens));
    const auto& selected_screens_ids =
        prefs->GetList(prefs::kChoobeSelectedScreens);

    EXPECT_EQ(selected_screens_ids, expected_selected_screens_ids_);
  } else {
    test::OobeJS().ExpectDisabledPath(kNextButtonPath);
    test::OobeJS().ClickOnPath(kSkipButtonPath);
    ChoobeScreen::Result result = WaitForScreenExitResult();

    EXPECT_EQ(result, ChoobeScreen::Result::SKIPPED);
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    EXPECT_FALSE(prefs->HasPrefPath(prefs::kChoobeSelectedScreens));
  }
}

}  // namespace ash
