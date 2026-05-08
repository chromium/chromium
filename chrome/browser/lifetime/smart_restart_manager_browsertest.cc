// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace smart_restart {

namespace {

class FakeUpgradeDetector : public UpgradeDetector {
 public:
  FakeUpgradeDetector()
      : UpgradeDetector(base::DefaultClock::GetInstance(),
                        base::DefaultTickClock::GetInstance()) {}

  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override {
    return base::Time();
  }

  void SetUpgradeAvailable() {
    set_upgrade_available(UPGRADE_AVAILABLE_REGULAR);
    NotifyUpgrade();
  }
};

}  // namespace

class SmartRestartManagerBrowserTest : public InProcessBrowserTest {
 public:
  SmartRestartManagerBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(features::kSmartRestart,
                                                     {{"restart_delay", "1s"}});
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Inject our fake detector into the manager.
    manager_ = std::make_unique<SmartRestartManager>(&fake_upgrade_detector_);
  }

  void TearDownOnMainThread() override {
    manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
  FakeUpgradeDetector fake_upgrade_detector_;
  std::unique_ptr<SmartRestartManager> manager_;
};

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest,
                       TriggersRestartOnZeroWindow) {
  base::HistogramTester histogram_tester;
  // 1. Setup: Pending update.
  fake_upgrade_detector_.SetUpgradeAvailable();

  // 2. Action: Close all windows.
  // We use a preference check because we can't easily intercept the exit(0)
  // call in a browser test without ending the test prematurely.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartInBackgroundOnShutdown));

  CloseBrowserSynchronously(browser());

  // 3. Verification: The manager should have set the background restart pref.
  // We use RunUntil to wait for the 1-second grace period timer to fire.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return local_state->GetBoolean(prefs::kRestartInBackgroundOnShutdown);
  }));

  EXPECT_TRUE(local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown));

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
      SmartRestartManager::ExecutionOutcome::kExecuted, 1);

  histogram_tester.ExpectTotalCount(
      "Session.SmartRestart.ZeroWindow.TimeSinceUpgradeDetected", 1);

  // Cleanup to prevent actual restart during teardown.
  browser_shutdown::SetTryingToQuit(false);
  local_state->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  local_state->SetBoolean(prefs::kRestartInBackgroundOnShutdown, false);
}

IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest,
                       CancelsRestartOnWindowOpen) {
  base::HistogramTester histogram_tester;
  // 1. Setup: Pending update.
  fake_upgrade_detector_.SetUpgradeAvailable();

  // 2. Action: Close all windows to start the timer.
  Profile* profile = browser()->profile();
  PrefService* local_state = g_browser_process->local_state();

  // Keep the process and profile alive between windows.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  CloseBrowserSynchronously(browser());

  // 3. Action: Quickly open a new window before the 1s timer fires.
  CreateBrowser(profile);
  EXPECT_GT(GlobalBrowserCollection::GetInstance()->GetSize(), 0u);

  // 4. Verification: Wait for 2 seconds (longer than the 1s delay).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // The restart should NOT have been triggered.
  EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartInBackgroundOnShutdown));

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
      SmartRestartManager::ExecutionOutcome::kCancelledByUser, 1);

  histogram_tester.ExpectTotalCount(
      "Session.SmartRestart.ZeroWindow.RemainingTimeAtCancellation", 1);
}

IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest,
                       BlockWhenProfilePickerOpen) {
  // 1. Setup: Pending update.
  fake_upgrade_detector_.SetUpgradeAvailable();

  // 2. Action: Open the Profile Picker.
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartup));
  profiles::testing::WaitForPickerWidgetCreated();
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // 3. Action: Close all browser windows.
  PrefService* local_state = g_browser_process->local_state();
  CloseBrowserSynchronously(browser());

  // 4. Verification: Wait for longer than the 1s delay.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // The restart should NOT have been triggered because the Profile Picker is
  // open.
  EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartInBackgroundOnShutdown));
}
#endif

}  // namespace smart_restart
