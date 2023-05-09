// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/kiosk_ash_browser_test_starter.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/new_aura_window_watcher.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

// The default kiosk app from `KioskBaseTest` uses `chrome.test` API which does
// not exist in Ash+Lacros tests. Chrome App Kiosk Ash+Lacros browser tests
// should use Kiosk Base Test App.
const char kKioskBaseTestAppId[] = "epancfbahpnkphlhpeefecinmgclhjlj";

}  // namespace

// Tests Ash-side of the chrome app kiosk when Lacros is enabled.
// To run these tests with pixel output, add
// `--lacros-chrome-additional-args=--gpu-sandbox-start-early` flag.
class ChromeAppKioskLacrosTest : public KioskBaseTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    if (kiosk_ash_starter_.HasLacrosArgument()) {
      kiosk_ash_starter_.PrepareEnvironmentForKioskLacros();
    }
    KioskBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    SetTestApp(kKioskBaseTestAppId);

    KioskBaseTest::SetUpOnMainThread();
    if (kiosk_ash_starter_.HasLacrosArgument()) {
      kiosk_ash_starter_.SetLacrosAvailabilityPolicy();
    }
  }

 protected:
  KioskAshBrowserTestStarter kiosk_ash_starter_;
};

IN_PROC_BROWSER_TEST_F(ChromeAppKioskLacrosTest, RegularOnlineKiosk) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }
  NewAuraWindowWatcher watcher;
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);

  aura::Window* window = watcher.WaitForWindow();
  KioskSessionInitializedWaiter().Wait();

  EXPECT_TRUE(crosapi::browser_util::IsLacrosWindow(window));
  EXPECT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
}

}  // namespace ash
