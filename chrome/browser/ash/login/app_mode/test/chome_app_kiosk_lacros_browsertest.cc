// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_ash_browser_test_starter.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/new_aura_window_watcher.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"

namespace ash {

namespace {

// The default kiosk app from `KioskBaseTest` uses `chrome.test` API which does
// not exist in Ash+Lacros tests. Chrome App Kiosk Ash+Lacros browser tests
// should use Kiosk Base Test App.
const char kKioskBaseTestAppId[] = "epancfbahpnkphlhpeefecinmgclhjlj";

const char kLaunchErrorHistogramName[] = "Kiosk.Launch.Error";

}  // namespace

// Tests Ash-side of the chrome app kiosk when Lacros is enabled.
// To run these tests with pixel output, add
// `--lacros-chrome-additional-args=--gpu-sandbox-start-early` flag.
class ChromeAppKioskLacrosTest : public KioskBaseTest {
 public:
  ChromeAppKioskLacrosTest() {
    // TODO(crbug.com/325314721): Remove this feature override once test-only
    // handling for the item snippets API is complete.
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kUseItemSnippetsAPI);
  }

  void SetUpInProcessBrowserTestFixture() override {
    kiosk_ash_starter_ = std::make_unique<KioskAshBrowserTestStarter>();
    if (kiosk_ash_starter_->HasLacrosArgument()) {
      kiosk_ash_starter_->PrepareEnvironmentForKioskLacros();
    }
    KioskBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    if (kiosk_ash_starter_->HasLacrosArgument()) {
      kiosk_ash_starter_.reset();
    }
  }

  void SetUpOnMainThread() override {
    SetTestApp(kKioskBaseTestAppId);

    KioskBaseTest::SetUpOnMainThread();
    if (kiosk_ash_starter_->HasLacrosArgument()) {
      kiosk_ash_starter_->SetLacrosAvailabilityPolicy();
      kiosk_ash_starter_->SetUpBrowserManager();
    }
  }

 protected:
  // Use a pointer so this can be initialized in
  // SetUpInProcessBrowserTestFixture, after `scoped_feature_list_`. This
  // prevents a crash where changes made from `scoped_feature_list_` causes a
  // mismatch in field trial states.
  std::unique_ptr<KioskAshBrowserTestStarter> kiosk_ash_starter_;
  base::HistogramTester histogram;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/324499540): Disable the test on ci/linux-chromeos-chrome-with-lacros
// since it is flaky.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_RegularOnlineKiosk DISABLED_RegularOnlineKiosk
#else
#define MAYBE_RegularOnlineKiosk RegularOnlineKiosk
#endif
IN_PROC_BROWSER_TEST_F(ChromeAppKioskLacrosTest, MAYBE_RegularOnlineKiosk) {
  if (!kiosk_ash_starter_->HasLacrosArgument()) {
    return;
  }
  NewAuraWindowWatcher watcher;
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);

  aura::Window* window = watcher.WaitForWindow();
  KioskSessionInitializedWaiter().Wait();

  EXPECT_TRUE(crosapi::browser_util::IsLacrosWindow(window));
  EXPECT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskLacrosTest, NonKioskAppLaunchError) {
  if (!kiosk_ash_starter_->HasLacrosArgument()) {
    return;
  }
  histogram.ExpectTotalCount(kLaunchErrorHistogramName, 0);

  SetTestApp(kTestNonKioskEnabledApp);
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);

  // App launch should be canceled, and kiosk session stopped.
  base::test::TestFuture<void> waiter;
  auto _ = browser_shutdown::AddAppTerminatingCallback(waiter.GetCallback());
  EXPECT_TRUE(waiter.Wait());

  // Checks the launch error is saved.
  EXPECT_EQ(KioskAppLaunchError::Error::kNotKioskEnabled,
            KioskAppLaunchError::Get());
}

// TODO(b/324499540): Disable the test on ci/linux-chromeos-chrome-with-lacros
// since it is flaky.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_ShouldLogPreviousLaunchError DISABLED_ShouldLogPreviousLaunchError
#else
#define MAYBE_ShouldLogPreviousLaunchError ShouldLogPreviousLaunchError
#endif
// Kiosk launch error is recorded on the next kiosk session run.
IN_PROC_BROWSER_TEST_F(ChromeAppKioskLacrosTest,
                       MAYBE_ShouldLogPreviousLaunchError) {
  if (!kiosk_ash_starter_->HasLacrosArgument()) {
    return;
  }

  KioskAppLaunchError::Save(KioskAppLaunchError::Error::kNotKioskEnabled);

  NewAuraWindowWatcher watcher;
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);

  watcher.WaitForWindow();
  KioskSessionInitializedWaiter().Wait();

  histogram.ExpectUniqueSample(kLaunchErrorHistogramName,
                               KioskAppLaunchError::Error::kNotKioskEnabled, 1);
}

}  // namespace ash
