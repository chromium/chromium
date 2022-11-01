// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/ash/login/wrong_hwid_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

class WrongHWIDScreenTest : public OobeBaseTest {
 public:
  WrongHWIDScreenTest() = default;
  ~WrongHWIDScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kForceHWIDCheckResultForTest,
                                    "failure");
    OobeBaseTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WrongHWIDScreenTest, BasicFlow) {
  OobeScreenWaiter(WrongHWIDScreenView::kScreenId).Wait();
  test::OobeJS().TapOnPath({"wrong-hwid", "skipButton"});
  OobeScreenExitWaiter(WrongHWIDScreenView::kScreenId).Wait();
}

}  // namespace
}  // namespace ash
