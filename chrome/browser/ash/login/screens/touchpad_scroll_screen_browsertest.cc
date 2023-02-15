// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

using ::testing::ElementsAre;

constexpr char kTouchpadScrollId[] = "touchpad-scroll";

const test::UIPath kToggleButtonPath = {kTouchpadScrollId, "updateToggle"};
const test::UIPath kNextButtonPath = {kTouchpadScrollId, "nextButton"};
const test::UIPath kTitlePath = {kTouchpadScrollId, "touchpad-scroll-title"};

}  // namespace

// TODO(crbug.com/269414043): Extend the test when policy and metrics are
// implemented.
class TouchpadScrollScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<test::UIPath> {
 public:
  TouchpadScrollScreenTest() {
    feature_list_.InitWithFeatures(
        {ash::features::kOobeChoobe, ash::features::kOobeTouchpadScroll}, {});
  }

  void SetUpOnMainThread() override {
    TouchpadScrollScreen* touchpad_scroll_screen =
        WizardController::default_controller()
            ->GetScreen<TouchpadScrollScreen>();

    original_callback_ =
        touchpad_scroll_screen->get_exit_callback_for_testing();
    touchpad_scroll_screen->set_exit_callback_for_testing(base::BindRepeating(
        &TouchpadScrollScreenTest::HandleScreenExit, base::Unretained(this)));
    touchpad_scroll_screen->set_ingore_pref_sync_for_testing(true);
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowTouchpadScrollScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        TouchpadScrollScreenView::kScreenId);
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }
    base::test::TestFuture<void> waiter;
    quit_closure_ = waiter.GetCallback();
    EXPECT_TRUE(waiter.Wait());
  }

  TouchpadScrollScreen::ScreenExitCallback original_callback_;
  absl::optional<TouchpadScrollScreen::Result> result_;

 protected:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  void HandleScreenExit(TouchpadScrollScreen::Result result) {
    result_ = result;
    original_callback_.Run(result);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;
};

IN_PROC_BROWSER_TEST_F(TouchpadScrollScreenTest, Next) {
  ShowTouchpadScrollScreen();

  // Check Screen is visible
  test::OobeJS().ExpectVisiblePath(kTitlePath);

  test::OobeJS().ClickOnPath(kNextButtonPath);
  WaitForScreenExit();

  EXPECT_EQ(result_.value(), TouchpadScrollScreen::Result::kNext);
}

IN_PROC_BROWSER_TEST_F(TouchpadScrollScreenTest, ToggleScrollDirectionOn) {
  ShowTouchpadScrollScreen();

  // Check Screen is visible
  test::OobeJS().ExpectVisiblePath(kTitlePath);

  // Check Toggle Button is visible and unchecked
  test::OobeJS().ExpectVisiblePath(kToggleButtonPath);
  test::OobeJS().ExpectHasNoAttribute("checked", kToggleButtonPath);
  test::OobeJS().ClickOnPath(kToggleButtonPath);

  test::OobeJS().ClickOnPath(kNextButtonPath);

  WaitForScreenExit();

  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetUserPrefValue(
          prefs::kNaturalScroll) != nullptr);

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kNaturalScroll));
  EXPECT_EQ(result_.value(), TouchpadScrollScreen::Result::kNext);
}

IN_PROC_BROWSER_TEST_F(TouchpadScrollScreenTest, ToggleScrollDirectionOff) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  // Set the pref of touchpad reverse scroll to enabled.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  profile->GetPrefs()->SetBoolean(prefs::kNaturalScroll, true);

  WizardController::default_controller()->AdvanceToScreen(
      TouchpadScrollScreenView::kScreenId);

  // Check Screen is visible
  test::OobeJS().ExpectVisiblePath(kTitlePath);

  // Check Toggle Button is visible and checked
  test::OobeJS().ExpectVisiblePath(kToggleButtonPath);
  test::OobeJS().ExpectHasAttribute("checked", kToggleButtonPath);

  test::OobeJS().ClickOnPath(kToggleButtonPath);

  test::OobeJS().ClickOnPath(kNextButtonPath);

  WaitForScreenExit();

  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetUserPrefValue(
          prefs::kNaturalScroll) != nullptr);

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kNaturalScroll));
  EXPECT_EQ(result_.value(), TouchpadScrollScreen::Result::kNext);
}

IN_PROC_BROWSER_TEST_F(TouchpadScrollScreenTest, RetainScrollDirectionOn) {
  login_manager_mixin_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  // Set the pref of touchpad reverse scroll to enabled.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  profile->GetPrefs()->SetBoolean(prefs::kNaturalScroll, true);

  WizardController::default_controller()->AdvanceToScreen(
      TouchpadScrollScreenView::kScreenId);

  // Check Screen is visible
  test::OobeJS().ExpectVisiblePath(kTitlePath);

  // Check Toggle Button is visible and checked
  test::OobeJS().ExpectVisiblePath(kToggleButtonPath);
  test::OobeJS().ExpectHasAttribute("checked", kToggleButtonPath);

  test::OobeJS().ExpectVisiblePath(kNextButtonPath);
  test::OobeJS().ClickOnPath(kNextButtonPath);

  WaitForScreenExit();

  EXPECT_TRUE(
      ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetUserPrefValue(
          prefs::kNaturalScroll) != nullptr);

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      prefs::kNaturalScroll));
  EXPECT_EQ(result_.value(), TouchpadScrollScreen::Result::kNext);
}

}  // namespace ash
