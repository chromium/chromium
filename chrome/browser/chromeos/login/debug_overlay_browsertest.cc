// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

constexpr char kDebugButton[] = "invokeDebuggerButton";
constexpr char kDebugOverlay[] = "debuggerOverlay";
constexpr char kScreensPanel[] = "DebuggerPanelScreens";

constexpr int kOobeScreensCount = 37;
constexpr int kLoginScreensCount = 33;

std::string ElementsInPanel(const std::string& panel) {
  return base::StrCat({"$('", panel, "').children.length"});
}

}  // namespace

class DebugOverlayTest : public OobeBaseTest {
 public:
  DebugOverlayTest() {}

  ~DebugOverlayTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::chromeos::switches::kShowOobeDevOverlay);
    OobeBaseTest::SetUpCommandLine(command_line);
  }
};

class DebugOverlayOnLoginTest : public DebugOverlayTest {
 public:
  DebugOverlayOnLoginTest() { login_mixin_.AppendRegularUsers(1); }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
  LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DebugOverlayTest, HideAndShow) {
  test::OobeJS().ExpectHidden(kDebugOverlay);
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(true, kDebugOverlay)->Wait();
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(false, kDebugOverlay)->Wait();
}

IN_PROC_BROWSER_TEST_F(DebugOverlayTest, ExpectScreenButtonsCount) {
  test::OobeJS().ExpectHidden(kDebugOverlay);
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(true, kDebugOverlay)->Wait();
  test::OobeJS().ExpectEQ(ElementsInPanel(kScreensPanel), kOobeScreensCount);
}

IN_PROC_BROWSER_TEST_F(DebugOverlayOnLoginTest, ExpectScreenButtonsCount) {
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  WaitForOobeUI();
  test::OobeJS().ExpectHidden(kDebugOverlay);
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(true, kDebugOverlay)->Wait();
  test::OobeJS().ExpectEQ(ElementsInPanel(kScreensPanel), kLoginScreensCount);
}

}  // namespace chromeos
