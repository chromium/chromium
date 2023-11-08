// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const test::UIPath kAutolaunchConfirmButton = {"autolaunch", "confirmButton"};
const test::UIPath kAutolaunchCancelButton = {"autolaunch", "cancelButton"};

void ConsumerKioskModeAutoStartLockCheck(bool* out_locked,
                                         base::OnceClosure runner_quit_task,
                                         bool in_locked) {
  LOG(INFO) << "kiosk locked  = " << in_locked;
  *out_locked = in_locked;
  std::move(runner_quit_task).Run();
}

void EnableConsumerKioskMode() {
  bool locked = false;
  base::RunLoop loop;
  KioskChromeAppManager::Get()->EnableConsumerKioskAutoLaunch(base::BindOnce(
      &ConsumerKioskModeAutoStartLockCheck, &locked, loop.QuitClosure()));
  loop.Run();
  EXPECT_TRUE(locked);
}

void WaitForAutoLaunchWarning(bool visibility) {
  test::OobeJS().CreateVisibilityWaiter(visibility, {"autolaunch"})->Wait();
}

}  // namespace

// The Kiosk Consumer mode is deprecated, but some customers use this mode to
// test their apps. So, it's better to keep these tests for now.
class KioskConsumerTest : public KioskBaseTest {
 public:
  KioskConsumerTest() { login_manager_.AppendRegularUsers(1); }

  // KioskBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    KioskBaseTest::SetUpCommandLine(command_line);
    // Postpone login screen creation.
    command_line->RemoveSwitch(switches::kForceLoginManagerInTests);
  }

  void SetUpInProcessBrowserTestFixture() override {
    KioskBaseTest::SetUpInProcessBrowserTestFixture();
    // Postpone login screen creation.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kForceLoginManagerInTests);
  }

  bool ShouldWaitForOobeUI() override { return false; }

 private:
  LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(KioskConsumerTest, AutolaunchWarningCancel) {
  EnableConsumerKioskMode();
  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskChromeAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskChromeAppManager::Get()->IsAutoLaunchEnabled());

  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);
  OobeScreenWaiter(KioskAutolaunchScreenView::kScreenId).Wait();

  // Wait for the auto launch warning come up.
  WaitForAutoLaunchWarning(/*visibility=*/true);
  test::OobeJS().ClickOnPath(kAutolaunchCancelButton);

  // Wait for the auto launch warning to go away.
  WaitForAutoLaunchWarning(/*visibility=*/false);

  EXPECT_FALSE(KioskChromeAppManager::Get()->IsAutoLaunchEnabled());
}

IN_PROC_BROWSER_TEST_F(KioskConsumerTest, AutolaunchWarningConfirm) {
  EnableConsumerKioskMode();
  ReloadAutolaunchKioskApps();
  EXPECT_FALSE(KioskChromeAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_FALSE(KioskChromeAppManager::Get()->IsAutoLaunchEnabled());

  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);
  OobeScreenWaiter(KioskAutolaunchScreenView::kScreenId).Wait();

  // Wait for the auto launch warning come up.
  WaitForAutoLaunchWarning(/*visibility=*/true);

  test::OobeJS().ClickOnPath(kAutolaunchConfirmButton);

  // Wait for the auto launch warning to go away.
  WaitForAutoLaunchWarning(/*visibility=*/false);

  EXPECT_FALSE(KioskChromeAppManager::Get()->GetAutoLaunchApp().empty());
  EXPECT_TRUE(KioskChromeAppManager::Get()->IsAutoLaunchEnabled());

  WaitForAppLaunchSuccess();

  KioskChromeAppManager::App app;
  ASSERT_TRUE(KioskChromeAppManager::Get()->GetApp(test_app_id(), &app));
  EXPECT_TRUE(app.was_auto_launched_with_zero_delay);
  EXPECT_EQ(ManifestLocation::kExternalPref, GetInstalledAppLocation());
}

// Verifies that a consumer device does not auto-launch kiosk mode when cros
// settings are untrusted.
IN_PROC_BROWSER_TEST_F(KioskConsumerTest, NoConsumerAutoLaunchWhenUntrusted) {
  EnableConsumerKioskMode();
  ReloadAutolaunchKioskApps();
  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);
  OobeScreenWaiter(KioskAutolaunchScreenView::kScreenId).Wait();

  WaitForAutoLaunchWarning(/*visibility=*/true);

  // Make cros settings untrusted.
  settings_helper_.SetTrustedStatus(
      CrosSettingsProvider::PERMANENTLY_UNTRUSTED);

  test::OobeJS().ClickOnPath(kAutolaunchConfirmButton);

  // Check that the attempt to auto-launch a kiosk app fails with an error.
  OobeScreenWaiter(LocalStateErrorScreenView::kScreenId).Wait();
}

}  // namespace ash
