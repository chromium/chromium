// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

class WrongHWIDScreenTest : public OobeBaseTest {
 public:
  WrongHWIDScreenTest() = default;
  ~WrongHWIDScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceHWIDCheckFailureForTest);
    OobeBaseTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WrongHWIDScreenTest, BasicFlow) {
  OobeScreenWaiter(WrongHWIDScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"wrong-hwid", "skipButton"});
  OobeScreenExitWaiter(WrongHWIDScreenView::kScreenId).Wait();
}

}  // namespace chromeos
