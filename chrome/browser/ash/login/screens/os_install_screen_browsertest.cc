// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

const test::UIPath kWelcomeScreen = {"connect", "welcomeScreen"};
const test::UIPath kOsInstallButton = {"connect", "welcomeScreen", "osInstall"};

}  // namespace

class OsInstallScreenTest : public OobeBaseTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowOsInstall);
  }
};

// If the kAllowOsInstall switch is not set, the welcome screen should
// not show the OS install button.
IN_PROC_BROWSER_TEST_F(OobeBaseTest, InstallButtonHiddenByDefault) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().ExpectHiddenPath(kOsInstallButton);
}

// If the kAllowOsInstall is set, the welcome screen should show the
// OS install button.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, InstallButtonVisibleWithSwitch) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(kWelcomeScreen);
  test::OobeJS().ExpectVisiblePath(kOsInstallButton);
}

// Clicking the OS install button should exit the welcome screen and
// start the OS install flow.
IN_PROC_BROWSER_TEST_F(OsInstallScreenTest, StartOsInstallFlow) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  test::OobeJS().TapOnPath(kOsInstallButton);

  OobeScreenWaiter(OsInstallScreenView::kScreenId).Wait();
}

}  // namespace chromeos
