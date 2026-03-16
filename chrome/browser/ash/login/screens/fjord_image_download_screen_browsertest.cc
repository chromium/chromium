// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_image_download_screen.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fjord_image_download_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kFjordImageDownloadElementId[] = "fjord-image-download";

const test::UIPath kFjordImageDownloadDialog = {kFjordImageDownloadElementId};

class FjordImageDownloadScreenBrowserTest : public OobeBaseTest {
 public:
  FjordImageDownloadScreenBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFjordOobeForceEnabled, features::kFjordOobeImageSwitch},
        {});
  }

  ~FjordImageDownloadScreenBrowserTest() override = default;

  void ShowFjordImageDownloadScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        FjordImageDownloadScreenView::kScreenId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FjordImageDownloadScreenBrowserTest, ScreenShown) {
  ShowFjordImageDownloadScreen();
  OobeScreenWaiter(FjordImageDownloadScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateVisibilityWaiter(true, kFjordImageDownloadDialog)
      ->Wait();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      FjordImageDownloadScreenView::kScreenId);
}

}  // namespace
}  // namespace ash
