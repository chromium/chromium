// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_touch_controller_screen.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fjord_touch_controller_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kFjordTouchControllerElementId[] = "fjord-touch-controller";

const test::UIPath kFjordTouchControllerDialog = {
    kFjordTouchControllerElementId};
const test::UIPath kNextButton = {kFjordTouchControllerElementId,
                                  "primaryButton"};
const test::UIPath kTestImageSkipButton = {kFjordTouchControllerElementId,
                                           "testImageSkipButton"};

class FjordTouchControllerScreenBrowserTest : public OobeBaseTest {
 public:
  FjordTouchControllerScreenBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kFjordOobeForceEnabled);
  }

  ~FjordTouchControllerScreenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    FjordTouchControllerScreen* screen =
        WizardController::default_controller()
            ->GetScreen<FjordTouchControllerScreen>();
    screen->set_exit_callback_for_testing(
        screen_exit_waiter_.GetRepeatingCallback());
  }

  void ShowFjordTouchControllerScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        FjordTouchControllerScreenView::kScreenId);
  }

  void WaitForScreenExit() { EXPECT_TRUE(screen_exit_waiter_.Wait()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TestFuture<void> screen_exit_waiter_;
};

IN_PROC_BROWSER_TEST_F(FjordTouchControllerScreenBrowserTest,
                       ScreenShownAndCanExit) {
  ShowFjordTouchControllerScreen();
  OobeScreenWaiter(FjordTouchControllerScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordTouchControllerDialog)
      ->Wait();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      FjordTouchControllerScreenView::kScreenId);
  EXPECT_TRUE(WizardController::default_controller()
                  ->GetScreen<FjordTouchControllerScreen>()
                  ->ExitScreen());
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FjordTouchControllerScreenBrowserTest,
                       NextButtonEnabledAfterSetupFinished) {
  ShowFjordTouchControllerScreen();
  OobeScreenWaiter(FjordTouchControllerScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordTouchControllerDialog)
      ->Wait();

  // The next button is disabled until the touch controller setup is finished.
  test::OobeJS().ExpectDisabledPath(kNextButton);

  EXPECT_TRUE(WizardController::default_controller()
                  ->OnFjordTouchControllerSetupFinished());
  test::OobeJS().CreateEnabledWaiter(true, kNextButton)->Wait();

  // The screen should not exit until the user clicks the next button.
  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      FjordTouchControllerScreenView::kScreenId);

  test::OobeJS().TapOnPath(kNextButton);
  WaitForScreenExit();
}

IN_PROC_BROWSER_TEST_F(FjordTouchControllerScreenBrowserTest,
                       TestImageSkipButtonHiddenOnNonTestImage) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_TRACK=stable-channel", base::Time());
  ShowFjordTouchControllerScreen();
  OobeScreenWaiter(FjordTouchControllerScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordTouchControllerDialog)
      ->Wait();

  test::OobeJS().ExpectHiddenPath(kTestImageSkipButton);
}

IN_PROC_BROWSER_TEST_F(FjordTouchControllerScreenBrowserTest,
                       TestImageSkipButtonShownAndExitsOnTestImage) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_TRACK=testimage-channel", base::Time());
  ShowFjordTouchControllerScreen();
  OobeScreenWaiter(FjordTouchControllerScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordTouchControllerDialog)
      ->Wait();

  // The skip button is visible and enabled.
  test::OobeJS().CreateVisibilityWaiter(true, kTestImageSkipButton)->Wait();
  test::OobeJS().ExpectEnabledPath(kTestImageSkipButton);

  test::OobeJS().TapOnPath(kTestImageSkipButton);
  WaitForScreenExit();
}

}  // namespace
}  // namespace ash
