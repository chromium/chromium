// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_info_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

class GaiaInfoScreenTest : public OobeBaseTest {
 public:
  GaiaInfoScreenTest() {
    feature_list_.InitAndEnableFeature(features::kOobeGaiaInfoScreen);
  }

  ~GaiaInfoScreenTest() override = default;

  void ShowGaiaInfoScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        GaiaInfoScreenView::kScreenId);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTest, ForwardFlow) {
  ShowGaiaInfoScreen();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"gaia-info", "nextButton"});
  OobeScreenExitWaiter(GaiaInfoScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenTest, BackFlow) {
  ShowGaiaInfoScreen();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"gaia-info", "backButton"});
  OobeScreenExitWaiter(GaiaInfoScreenView::kScreenId).Wait();
}

}  // namespace
}  // namespace ash
