// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_fw_update_screen.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/fjord_fw_update_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kFjordFwUpdateElementId[] = "fjord-fw-update";

const test::UIPath kFjordFwUpdateDialog = {kFjordFwUpdateElementId};

class FjordFwUpdateScreenBrowserTest : public OobeBaseTest {
 public:
  FjordFwUpdateScreenBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kFjordOobeForceEnabled);
  }

  ~FjordFwUpdateScreenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    FjordFwUpdateScreen* screen = WizardController::default_controller()
                                      ->GetScreen<FjordFwUpdateScreen>();
    screen->set_exit_callback_for_testing(
        screen_exit_waiter_.GetRepeatingCallback());
  }

  void ShowFjordFwUpdateScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        FjordFwUpdateScreenView::kScreenId);
  }

  void WaitForScreenExit() { EXPECT_TRUE(screen_exit_waiter_.Wait()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TestFuture<void> screen_exit_waiter_;
};

IN_PROC_BROWSER_TEST_F(FjordFwUpdateScreenBrowserTest, ScreenShownAndCanExit) {
  ShowFjordFwUpdateScreen();
  OobeScreenWaiter(FjordFwUpdateScreenView::kScreenId).Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kFjordFwUpdateDialog)->Wait();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      FjordFwUpdateScreenView::kScreenId);
  EXPECT_TRUE(WizardController::default_controller()
                  ->GetScreen<FjordFwUpdateScreen>()
                  ->ExitScreen());
  WaitForScreenExit();
}

}  // namespace
}  // namespace ash
