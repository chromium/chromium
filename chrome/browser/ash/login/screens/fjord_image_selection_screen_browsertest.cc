// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_image_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fjord_image_selection_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kFjordImageSelectionElementId[] = "fjord-image-selection";

const test::UIPath kFjordImageSelectionDialog = {kFjordImageSelectionElementId};
const test::UIPath kMeetButton = {kFjordImageSelectionElementId, "meetButton"};
const test::UIPath kZoomButton = {kFjordImageSelectionElementId, "zoomButton"};
const test::UIPath kNextButton = {kFjordImageSelectionElementId, "nextButton"};

class FjordImageSelectionScreenBrowserTest : public OobeBaseTest {
 public:
  FjordImageSelectionScreenBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFjordOobeForceEnabled, features::kFjordOobeImageSwitch},
        {});
  }

  ~FjordImageSelectionScreenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    FjordImageSelectionScreen* screen =
        WizardController::default_controller()
            ->GetScreen<FjordImageSelectionScreen>();
    screen->set_exit_callback_for_testing(
        screen_exit_waiter_.GetRepeatingCallback());
  }

  void ShowFjordImageSelectionScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        FjordImageSelectionScreenView::kScreenId);
  }

  FjordImageSelectionScreen::Result WaitForScreenExit() {
    return screen_exit_waiter_.Take();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TestFuture<FjordImageSelectionScreen::Result> screen_exit_waiter_;
};

IN_PROC_BROWSER_TEST_F(FjordImageSelectionScreenBrowserTest, ScreenShown) {
  ShowFjordImageSelectionScreen();
  OobeScreenWaiter(FjordImageSelectionScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordImageSelectionDialog)
      ->Wait();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      FjordImageSelectionScreenView::kScreenId);
}

// Verifies that selecting the Meet radio button and clicking Next exits the
// screen with the kCuttlefish result.
IN_PROC_BROWSER_TEST_F(FjordImageSelectionScreenBrowserTest,
                       SelectMeetAndClickNext) {
  ShowFjordImageSelectionScreen();
  OobeScreenWaiter(FjordImageSelectionScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordImageSelectionDialog)
      ->Wait();

  test::OobeJS().ClickOnPath(kMeetButton);
  test::OobeJS().ClickOnPath(kNextButton);

  FjordImageSelectionScreen::Result result = WaitForScreenExit();
  EXPECT_EQ(result, FjordImageSelectionScreen::Result::kCuttlefish);
}

// Verifies that selecting the Zoom radio button and clicking Next exits the
// screen with the kSquid result.
IN_PROC_BROWSER_TEST_F(FjordImageSelectionScreenBrowserTest,
                       SelectZoomAndClickNext) {
  ShowFjordImageSelectionScreen();
  OobeScreenWaiter(FjordImageSelectionScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordImageSelectionDialog)
      ->Wait();

  test::OobeJS().ClickOnPath(kZoomButton);
  test::OobeJS().ClickOnPath(kNextButton);

  FjordImageSelectionScreen::Result result = WaitForScreenExit();
  EXPECT_EQ(result, FjordImageSelectionScreen::Result::kSquid);
}

// Verifies that the next button is disabled by default when no radio button
// is selected.
IN_PROC_BROWSER_TEST_F(FjordImageSelectionScreenBrowserTest,
                       NextButtonDisabledByDefault) {
  ShowFjordImageSelectionScreen();
  OobeScreenWaiter(FjordImageSelectionScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordImageSelectionDialog)
      ->Wait();

  // Verify the next button is disabled when no selection is made.
  test::OobeJS().ExpectDisabledPath(kNextButton);

  // Select Meet and verify the button becomes enabled.
  test::OobeJS().ClickOnPath(kMeetButton);
  test::OobeJS().ExpectEnabledPath(kNextButton);
}

}  // namespace
}  // namespace ash
