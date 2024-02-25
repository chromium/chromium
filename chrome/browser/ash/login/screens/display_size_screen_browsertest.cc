// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/util/display_manager_util.h"

namespace ash {

namespace {

using ::testing::ElementsAre;

constexpr char kDispalaySizeId[] = "display-size";
constexpr char kSizeSelector[] = "sizeSelector";
const test::UIPath kIncreaseButton = {kDispalaySizeId, kSizeSelector,
                                      "positiveButton"};
const test::UIPath kDecreaseButton = {kDispalaySizeId, kSizeSelector,
                                      "negativeButton"};
const test::UIPath kSizeSlider = {kDispalaySizeId, kSizeSelector, "sizeSlider"};
const test::UIPath kNextButton = {kDispalaySizeId, "nextButton"};

}  // namespace

// TODO(b/269414043): Extend the test when policy and metrics are implemented.
class DisplaySizeScreenTest : public OobeBaseTest {
 public:
  DisplaySizeScreenTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kOobeChoobe, ash::features::kOobeDisplaySize}, {});
  }

  void SetUpOnMainThread() override {
    DisplaySizeScreen* display_size_screen =
        WizardController::default_controller()->GetScreen<DisplaySizeScreen>();

    original_callback_ = display_size_screen->get_exit_callback_for_testing();
    display_size_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowDisplaySizeScreen() {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->skip_choobe_for_tests = true;

    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        DisplaySizeScreenView::kScreenId);
  }

  DisplaySizeScreen::Result WaitForScreenExitResult() {
    DisplaySizeScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

  std::vector<float> GetAvailableSizes() {
    const auto display_id =
        display::Screen::GetScreen()->GetPrimaryDisplay().id();
    const auto& info =
        ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
    auto factors = display::GetDisplayZoomFactors(info.display_modes()[0]);
    return factors;
  }

  int GetCurrentSizeIndex() {
    const auto display_id =
        display::Screen::GetScreen()->GetPrimaryDisplay().id();
    const auto& info =
        ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
    float current_size = info.zoom_factor();
    auto available_sizes = GetAvailableSizes();
    int current_size_index = 0;
    for (int i = 1; i < (int)available_sizes.size(); i++) {
      if (abs(current_size - available_sizes[i]) <
          abs(current_size - available_sizes[current_size_index])) {
        current_size_index = i;
      }
    }
    return current_size_index;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::TestFuture<DisplaySizeScreen::Result> screen_result_waiter_;
  DisplaySizeScreen::ScreenExitCallback original_callback_;
};

IN_PROC_BROWSER_TEST_F(DisplaySizeScreenTest, InitialSliderValue) {
  ShowDisplaySizeScreen();
  OobeScreenWaiter(DisplaySizeScreenView::kScreenId).Wait();

  test::OobeJS().ExpectAttributeEQ("value", kSizeSlider, GetCurrentSizeIndex());

  test::OobeJS().TapOnPath(kNextButton);
  DisplaySizeScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, DisplaySizeScreen::Result::kNext);

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->HasPrefPath(prefs::kOobeDisplaySizeFactorDeferred));
}

IN_PROC_BROWSER_TEST_F(DisplaySizeScreenTest, PrefUpdatedMaxSize) {
  ShowDisplaySizeScreen();
  OobeScreenWaiter(DisplaySizeScreenView::kScreenId).Wait();

  // Click the increase button enough times to reach the max display size.
  auto sizes = GetAvailableSizes();
  for (int i = 0; i < (int)sizes.size(); i++) {
    test::OobeJS().TapOnPath(kIncreaseButton);
  }

  test::OobeJS().TapOnPath(kNextButton);
  DisplaySizeScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, DisplaySizeScreen::Result::kNext);

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->HasPrefPath(prefs::kOobeDisplaySizeFactorDeferred));

  float pref_size = prefs->GetDouble(prefs::kOobeDisplaySizeFactorDeferred);
  EXPECT_FLOAT_EQ(sizes.back(), pref_size);
}

IN_PROC_BROWSER_TEST_F(DisplaySizeScreenTest, PrefUpdatedMinSize) {
  ShowDisplaySizeScreen();
  OobeScreenWaiter(DisplaySizeScreenView::kScreenId).Wait();

  // Click the decrease button enough times to reach the min display size.
  auto sizes = GetAvailableSizes();
  for (int i = 0; i < (int)sizes.size(); i++) {
    test::OobeJS().TapOnPath(kDecreaseButton);
  }

  test::OobeJS().TapOnPath(kNextButton);
  DisplaySizeScreen::Result result = WaitForScreenExitResult();
  EXPECT_EQ(result, DisplaySizeScreen::Result::kNext);

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->HasPrefPath(prefs::kOobeDisplaySizeFactorDeferred));

  float pref_size = prefs->GetDouble(prefs::kOobeDisplaySizeFactorDeferred);
  EXPECT_FLOAT_EQ(sizes.front(), pref_size);
}
}  // namespace ash
