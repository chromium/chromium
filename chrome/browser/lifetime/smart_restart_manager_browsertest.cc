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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
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
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace smart_restart {

namespace {

using Blocker = ExtendedRestartabilityState::SmartRestartBlocker;

class PageNodeInteractionWaiter : public performance_manager::PageNodeObserver {
 public:
  explicit PageNodeInteractionWaiter(content::WebContents* contents)
      : target_contents_(contents) {
    observation_.Observe(performance_manager::PerformanceManager::GetGraph());
  }

  void Wait() {
    auto page_node = performance_manager::PerformanceManager::
        GetPrimaryPageNodeForWebContents(target_contents_);
    if (page_node && page_node->HadFormInteraction()) {
      return;
    }
    run_loop_.Run();
  }

  void OnHadFormInteractionChanged(
      const performance_manager::PageNode* page_node) override {
    if (page_node->GetWebContents().get() == target_contents_ &&
        page_node->HadFormInteraction() && run_loop_.running()) {
      run_loop_.Quit();
    }
  }

 private:
  const raw_ptr<content::WebContents> target_contents_;
  base::RunLoop run_loop_;
  base::ScopedObservation<performance_manager::Graph,
                          performance_manager::PageNodeObserver>
      observation_{this};
};

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

class SmartRestartManagerTestBase : public InProcessBrowserTest {
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

class SmartRestartManagerBrowserTest : public SmartRestartManagerTestBase {
 public:
  SmartRestartManagerBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSmartRestart, {{"restart_delay", "1s"}}},
         {features::kSmartRestartLockScreen, {{"lock_restart_delay", "1s"}}}},
        {});
  }
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
      ExecutionOutcome::kExecuted, 1);

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
      ExecutionOutcome::kCancelledByUser, 1);

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

IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest,
                       HandlesOverlappingTimers_LockThenZeroWindow) {
  base::HistogramTester histogram_tester;

  // 1. Keep the browser process alive even when all windows close.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      browser()->profile(), ProfileKeepAliveOrigin::kBrowserWindow);

  // 2. Setup: 1 window open and a pending update.
  fake_upgrade_detector_.SetUpgradeAvailable();

  // 2. Action 1: Simulate Lock. (Starts Lock Screen timer).
  manager_->SetLockedStateForTesting(true);

  // 3. Action 2: Close the last window. (Starts Zero Window timer).
  CloseBrowserSynchronously(browser());

  // 4. Wait for both timers to fire (both have 1s delay in tests).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // 5. Verify that only ONE restart was executed.
  int zero_window_executions = histogram_tester.GetBucketCount(
      "Session.SmartRestart.ZeroWindow.ExecutionOutcome",
      static_cast<int>(ExecutionOutcome::kExecuted));

  int lock_screen_executions = histogram_tester.GetBucketCount(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kExecuted));

  EXPECT_EQ(1, zero_window_executions + lock_screen_executions);

  // Cleanup to prevent actual restart during teardown.
  PrefService* local_state = g_browser_process->local_state();
  browser_shutdown::SetTryingToQuit(false);
  local_state->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  local_state->SetBoolean(prefs::kRestartInBackgroundOnShutdown, false);
}
#endif  // BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest,
                       TriggersRestartOnLockScreen) {
  base::HistogramTester histogram_tester;
  Profile* profile = browser()->profile();

  // 1. Setup: Pending update.
  fake_upgrade_detector_.SetUpgradeAvailable();

  // 2. Action: Simulate Lock.
  PrefService* local_state = g_browser_process->local_state();
  EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartInBackgroundOnShutdown));

  manager_->SetLockedStateForTesting(true);

  // 3. Verification: Wait for the 1-second grace period timer to fire.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown);
  }));

  ASSERT_TRUE(profile);
  const auto& dict =
      profile->GetPrefs()->GetDict(prefs::kPreSmartRestartSessionState);
  EXPECT_EQ(dict.FindInt(SessionRestore::kNormalTabsKey).value_or(0), 1);
  EXPECT_EQ(dict.FindInt(SessionRestore::kNormalWindowsKey).value_or(0), 1);
  EXPECT_FALSE(dict.FindInt(SessionRestore::kAppTabsKey).has_value());
  EXPECT_FALSE(dict.FindInt(SessionRestore::kAppWindowsKey).has_value());

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kExecuted), 1);

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome.LowTab",
      static_cast<int>(ExtendedExecutionOutcome::kExecuted), 1);

  histogram_tester.ExpectTotalCount(
      "Session.SmartRestart.Lock.TimeSinceUpgradeDetected", 1);

  // Cleanup.
  browser_shutdown::SetTryingToQuit(false);
  local_state->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  local_state->SetBoolean(prefs::kRestartInBackgroundOnShutdown, false);
}

IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest,
                       BlockOnLockScreenHighDisruption) {
  base::HistogramTester histogram_tester;
  // 1. Setup: Pending update and a "High Disruption" blocker (e.g. Incognito).
  fake_upgrade_detector_.SetUpgradeAvailable();
  CreateIncognitoBrowser(browser()->profile());

  // 2. Action: Simulate Lock.
  PrefService* local_state = g_browser_process->local_state();
  manager_->SetLockedStateForTesting(true);

  // 3. Verification: Wait for 2 seconds (longer than the 1s delay).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // The restart should NOT have been triggered.
  EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown));

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome.LowTab",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);
}

IN_PROC_BROWSER_TEST_F(SmartRestartManagerBrowserTest, CancelsRestartOnUnlock) {
  base::HistogramTester histogram_tester;

  // 1. Setup: Pending update.
  fake_upgrade_detector_.SetUpgradeAvailable();

  // 2. Action: Simulate Lock.
  manager_->SetLockedStateForTesting(true);

  // Wait for 500ms (the grace period in tests is 1s).
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
  run_loop.Run();

  // 3. Action: Simulate Unlock.
  manager_->SetLockedStateForTesting(false);

  // 4. Verification: Telemetry.
  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kCancelledByUser), 1);

  // Verify that the remaining time was logged!
  histogram_tester.ExpectTotalCount(
      "Session.SmartRestart.Lock.RemainingTimeAtCancellation", 1);
}

class SmartRestartManagerConservativeBrowserTest
    : public SmartRestartManagerTestBase {
 public:
  SmartRestartManagerConservativeBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSmartRestartLockScreen,
          {{"lock_disruption_threshold", "0"}, {"lock_restart_delay", "1s"}}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(SmartRestartManagerConservativeBrowserTest,
                       BlockOnLockScreen_TabBlockers) {
  base::HistogramTester histogram_tester;

  // 1. Setup Tab 1: Pinned.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  browser()->tab_strip_model()->SetTabPinned(0, true);

  // 2. Setup Tab 2: PDF Mime Type.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("data:application/pdf,test"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  fake_upgrade_detector_.SetUpgradeAvailable();
  manager_->SetLockedStateForTesting(true);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // 4. Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome.LowTab",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason",
      static_cast<int>(Blocker::kPinnedTab), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason.LowTab",
      static_cast<int>(Blocker::kPinnedTab), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason",
      static_cast<int>(Blocker::kPdf), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason.LowTab",
      static_cast<int>(Blocker::kPdf), 1);
}

IN_PROC_BROWSER_TEST_F(SmartRestartManagerConservativeBrowserTest,
                       BlockOnLockScreen_PageNodeBlockers) {
  base::HistogramTester histogram_tester;

  // 1. Setup session: Navigate to a page with an input field.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<html><body><input id='t'></body></html>")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  PageNodeInteractionWaiter waiter(web_contents);

  // 2. Simulate form interaction.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "document.getElementById('t').focus();"));
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('a'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);

  waiter.Wait();

  // 3. Simulate Lock.
  fake_upgrade_detector_.SetUpgradeAvailable();
  manager_->SetLockedStateForTesting(true);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // 4. Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome.LowTab",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason",
      static_cast<int>(Blocker::kFormInteractions), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason.LowTab",
      static_cast<int>(Blocker::kFormInteractions), 1);
}

IN_PROC_BROWSER_TEST_F(SmartRestartManagerConservativeBrowserTest,
                       BlockOnLockScreen_OpportunityMetrics) {
  base::HistogramTester histogram_tester;

  // 1. Setup session: 1 tab, simulate video capture (High Disruption).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body>Capturing</body></html>")));
  performance_manager::PageLiveStateDecorator::OnIsCapturingVideoChanged(
      browser()->tab_strip_model()->GetActiveWebContents(), true);

  // 2. Simulate Lock.
  fake_upgrade_detector_.SetUpgradeAvailable();
  manager_->SetLockedStateForTesting(true);

  // Wait for timer...
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
  run_loop.Run();

  // 3. Verify metrics.
  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  histogram_tester.ExpectUniqueSample(
      "Session.SmartRestart.Lock.ExecutionOutcome.LowTab",
      static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel), 1);

  // Verify specific blocker is present.
  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason",
      static_cast<int>(Blocker::kCapturingVideo), 1);

  histogram_tester.ExpectBucketCount(
      "Session.SmartRestart.Lock.ProtectionReason.LowTab",
      static_cast<int>(Blocker::kCapturingVideo), 1);
}

class SmartRestartManagerBypassBeforeUnloadParameterizedTest
    : public SmartRestartManagerTestBase,
      public ::testing::WithParamInterface<double> {
 public:
  SmartRestartManagerBypassBeforeUnloadParameterizedTest() {
    double threshold = GetParam();
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kSmartRestart, {{"restart_delay", "1s"}}},
         {features::kSmartRestartLockScreen,
          {{"lock_restart_delay", "1s"},
           {"lock_bypass_beforeunload_threshold",
            base::NumberToString(threshold)}}}},
        {});
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SmartRestartManagerBypassBeforeUnloadParameterizedTest,
    ::testing::Values(-1.0,
                      15.0,
                      site_engagement::SiteEngagementScore::kMaxPoints + 1.0));

IN_PROC_BROWSER_TEST_P(SmartRestartManagerBypassBeforeUnloadParameterizedTest,
                       LockScreenRestartWithBeforeUnload) {
  base::HistogramTester histogram_tester;
  PrefService* local_state = g_browser_process->local_state();

  // Navigate to a page and add a beforeunload handler + user activation.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(tab, "window.onbeforeunload = () => 'draft';"));
  content::PrepContentsForBeforeUnloadTest(tab);

  // Setup: Pending update and lock screen.
  fake_upgrade_detector_.SetUpgradeAvailable();
  EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown));

  manager_->SetLockedStateForTesting(true);

  double threshold = GetParam();
  if (threshold < 0.0) {
    // Range 1 (Disabled): The restart is blocked, and beforeunload keeps High
    // Disruption.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
    run_loop.Run();

    EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown));

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.ExecutionOutcome",
        static_cast<int>(ExtendedExecutionOutcome::kBlockedByDisruptionLevel),
        1);

    histogram_tester.ExpectBucketCount(
        "Session.SmartRestart.Lock.ProtectionReason",
        static_cast<int>(Blocker::kBeforeUnloadHandler), 1);

    // Cleanup to prevent beforeunload prompt from blocking test teardown.
    ASSERT_TRUE(content::ExecJs(tab, "window.onbeforeunload = null;"));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  } else if (threshold <= site_engagement::SiteEngagementScore::kMaxPoints) {
    // Range 2 (Live Gate): The restart executes successfully, invoking
    // RelaunchIgnoreUnloadHandlers. In our test case the tab's score is 0.0
    // which is <= threshold (15.0).
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown);
    }));

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.ExecutionOutcome",
        static_cast<int>(ExtendedExecutionOutcome::kExecuted), 1);

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.SuppressedBeforeUnloadRatio", 100, 1);

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.SuppressedBeforeUnloadCount", 1, 1);

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.SuppressedBeforeUnloadEngagementScore", 0,
        1);

    // Cleanup to prevent actual restart during teardown.
    browser_shutdown::SetTryingToQuit(false);
    local_state->SetBoolean(prefs::kRestartLastSessionOnShutdown, false);
  } else {
    // Range 3 (Dry-Run): The relaunch is intercepted, metrics are logged, and
    // returns without relaunching.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(2));
    run_loop.Run();

    EXPECT_FALSE(local_state->GetBoolean(prefs::kRestartLastSessionOnShutdown));

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.ExecutionOutcome",
        static_cast<int>(ExtendedExecutionOutcome::kExecuted), 1);

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.SuppressedBeforeUnloadRatio", 100, 1);

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.SuppressedBeforeUnloadCount", 1, 1);

    histogram_tester.ExpectUniqueSample(
        "Session.SmartRestart.Lock.SuppressedBeforeUnloadEngagementScore", 0,
        1);

    // Cleanup to prevent beforeunload prompt from blocking test teardown.
    ASSERT_TRUE(content::ExecJs(tab, "window.onbeforeunload = null;"));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace smart_restart
