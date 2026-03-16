// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_image_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
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

class FjordImageSelectionScreenBrowserTest : public OobeBaseTest {
 public:
  FjordImageSelectionScreenBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFjordOobeForceEnabled, features::kFjordOobeImageSwitch},
        {});
  }

  ~FjordImageSelectionScreenBrowserTest() override = default;

  void ShowFjordImageSelectionScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        FjordImageSelectionScreenView::kScreenId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

}  // namespace
}  // namespace ash
