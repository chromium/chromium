// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_trial_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

const test::UIPath kWelcomeGetStartedButton = {"connect", "welcomeScreen",
                                               "getStarted"};
const test::UIPath kOsInstallBackButton = {"os-install", "osInstallExitButton"};

const test::UIPath kInstallRadioButton = {"os-trial", "installButton"};
const test::UIPath kTryRadioButton = {"os-trial", "tryButton"};
const test::UIPath kNextButton = {"os-trial", "nextButton"};
const test::UIPath kBackButton = {"os-trial", "backButton"};
const test::UIPath kOsTrialDialog = {"os-trial", "osTrialDialog"};

}  // namespace

class OsTrialScreenTest : public OobeBaseTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowOsInstall);
  }

  void ShowOsTrialScreen() {
    test::WaitForWelcomeScreen();
    test::OobeJS().TapOnPath(kWelcomeGetStartedButton);
    OobeScreenWaiter(OsTrialScreenView::kScreenId).Wait();
    test::OobeJS().ExpectHasAttribute("checked", kInstallRadioButton);
  }

  void SelectTrialOption(test::UIPath element_id) {
    test::OobeJS().ExpectVisiblePath(kOsTrialDialog);
    test::OobeJS().ClickOnPath(element_id);
    test::OobeJS().ExpectHasAttribute("checked", element_id);
    test::OobeJS().ClickOnPath(kNextButton);
  }
};

// Check that the selection of the try option continues OOBE flow.
IN_PROC_BROWSER_TEST_F(OsTrialScreenTest, TryOptionSelected) {
  ShowOsTrialScreen();
  SelectTrialOption(kTryRadioButton);
  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();
}

// Check that the selection of the install option continues to `OS install`
// screen.
IN_PROC_BROWSER_TEST_F(OsTrialScreenTest, InstallOptionSelected) {
  ShowOsTrialScreen();
  SelectTrialOption(kInstallRadioButton);
  OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();
}

// Clicking back in `OS trial` screen should return the user to the welcome
// screen.
IN_PROC_BROWSER_TEST_F(OsTrialScreenTest, BackNavigation) {
  ShowOsTrialScreen();
  test::OobeJS().ClickOnPath(kBackButton);
  test::WaitForWelcomeScreen();
}

// If `Start OS Install` button was clicked from the shelf in the user creation
// screen, the trial screen should be skipped.
IN_PROC_BROWSER_TEST_F(OsTrialScreenTest, TrialScreenSkipped) {
  WizardController::default_controller()->AdvanceToScreen(
      UserCreationView::kScreenId);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  ASSERT_TRUE(LoginScreenTestApi::IsOsInstallButtonShown());
  ASSERT_TRUE(LoginScreenTestApi::ClickOsInstallButton());
  OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();
}

// If `OS trial` screen was shown, clicking back in the `OS Istall` screen
// should show the `OS Trial` screen.
IN_PROC_BROWSER_TEST_F(OsTrialScreenTest, OsInstallBackNavigationTrialShown) {
  ShowOsTrialScreen();
  SelectTrialOption(kInstallRadioButton);
  test::OobeJS().ClickOnPath(kOsInstallBackButton);
  OobeScreenWaiter(OsTrialScreenView::kScreenId).Wait();
}

// If `OS trial` screen was skipped due to clicking `Start OS Install` button
// from the shelf in the user creation screen, clicking back in the `OS Istall`
// screen should show the user creation screen.
IN_PROC_BROWSER_TEST_F(OsTrialScreenTest, OsInstallBackNavigationTrialSkipped) {
  WizardController::default_controller()->AdvanceToScreen(
      UserCreationView::kScreenId);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  ASSERT_TRUE(LoginScreenTestApi::ClickOsInstallButton());
  OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();

  test::OobeJS().ClickOnPath(kOsInstallBackButton);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
}
}  // namespace ash
