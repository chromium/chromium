// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kDebugButton[] = "invokeDebuggerButton";
constexpr char kDebugOverlay[] = "debuggerOverlay";
constexpr char kScreensPanel[] = "DebuggerPanelScreens";

constexpr int kCommonScreensCount = 53;
constexpr int kOobeOnlyScreensCount = 10;
constexpr int kLoginOnlyScreensCount = 4;

constexpr int kOobeScreensCount = kCommonScreensCount + kOobeOnlyScreensCount;
constexpr int kLoginScreensCount = kCommonScreensCount + kLoginOnlyScreensCount;

// Feature-specific screens:
constexpr int kOsInstallScreensCount = 2;

std::string ElementsInPanel(const std::string& panel) {
  return base::StrCat({"$('", panel, "').children.length"});
}

}  // namespace

class DebugOverlayTest : public OobeBaseTest {
 public:
  DebugOverlayTest() {
    feature_list_.InitWithFeatures(
        {features::kOobeChoobe, features::kOobeTouchpadScroll,
         features::kOobeDisplaySize, features::kOobeGaiaInfoScreen,
         features::kOobeSoftwareUpdate, features::kOobePersonalizedOnboarding,
         features::kOobePerksDiscovery,
         features::kOobeSplitModifierKeyboardInfo},
        {});
  }

  ~DebugOverlayTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kShowOobeDevOverlay);
    OobeBaseTest::SetUpCommandLine(command_line);
  }

  base::test::ScopedFeatureList feature_list_;
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
  WaitForOobeUI();
  test::OobeJS().ExpectHidden(kDebugOverlay);
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(true, kDebugOverlay)->Wait();
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(false, kDebugOverlay)->Wait();
}

class DebugOverlayScreensTest : public DebugOverlayTest,
                                /* IsOsInstallAllowed */
                                public ::testing::WithParamInterface<bool> {
 public:
  DebugOverlayScreensTest() = default;
  ~DebugOverlayScreensTest() override = default;
  // DebugOverlayTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DebugOverlayTest::SetUpCommandLine(command_line);
    if (!GetParam()) {
      return;
    }
    command_line->AppendSwitch(switches::kAllowOsInstall);
  }
};

IN_PROC_BROWSER_TEST_P(DebugOverlayScreensTest, ExpectScreenButtonsCount) {
  WaitForOobeUI();
  test::OobeJS().ExpectHidden(kDebugOverlay);
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(true, kDebugOverlay)->Wait();

  int screens_count = kOobeScreensCount;
  if (switches::IsOsInstallAllowed()) {
    screens_count += kOsInstallScreensCount;
  }

  test::OobeJS().ExpectEQ(ElementsInPanel(kScreensPanel), screens_count);
}

/* No makes it easier to run all tests with one filter */
INSTANTIATE_TEST_SUITE_P(, DebugOverlayScreensTest, testing::Bool());

IN_PROC_BROWSER_TEST_F(DebugOverlayOnLoginTest, ExpectScreenButtonsCount) {
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  WaitForOobeUI();
  test::OobeJS().ExpectHidden(kDebugOverlay);
  test::OobeJS().ExpectVisible(kDebugButton);
  test::OobeJS().ClickOn(kDebugButton);
  test::OobeJS().CreateVisibilityWaiter(true, kDebugOverlay)->Wait();

  test::OobeJS().ExpectEQ(ElementsInPanel(kScreensPanel), kLoginScreensCount);
}

}  // namespace ash
