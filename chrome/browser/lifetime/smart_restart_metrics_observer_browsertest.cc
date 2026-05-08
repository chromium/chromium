// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_metrics_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/lifetime/restartability_monitor.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace smart_restart {
namespace {

// A fake UpgradeDetector to control update availability in tests without
// triggering actual restarts.
class FakeUpgradeDetector : public UpgradeDetector {
 public:
  FakeUpgradeDetector()
      : UpgradeDetector(base::DefaultClock::GetInstance(),
                        base::DefaultTickClock::GetInstance()) {
    set_upgrade_detected_time(this->clock()->Now());
  }

  // UpgradeDetector overrides:
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override {
    return base::Time();
  }

  void SetUpgradeAvailableToRegular() {
    set_upgrade_available(UPGRADE_AVAILABLE_REGULAR);
    NotifyUpgrade();
  }
};

}  // namespace

class SmartRestartMetricsObserverBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Create an observer for testing, injecting our fake detector.
    observer_ =
        std::make_unique<SmartRestartMetricsObserver>(&fake_upgrade_detector_);
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool TestUpdateAvailable() const { return GetParam(); }

  FakeUpgradeDetector fake_upgrade_detector_;
  std::unique_ptr<SmartRestartMetricsObserver> observer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SmartRestartMetricsObserverBrowserTest,
                         testing::Bool());

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(SmartRestartMetricsObserverBrowserTest,
                       ZeroWindowDuration) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Session.ZeroWindowDuration", 0);

  // Simulate an upgrade available.
  if (TestUpdateAvailable()) {
    fake_upgrade_detector_.SetUpgradeAvailableToRegular();
  }

  // Close the default browser window the test starts with.
  Profile* profile = browser()->profile();

  // Keep the process and profile alive between windows.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  CloseBrowserSynchronously(browser());

  // Open a new window. This ends the Zero Window state.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CreateBrowser(profile);
  }

  // Verify the metric ran.
  // Note: We expect two samples here because both the real production observer
  // and the test observer are listening to the BrowserList.
  histogram_tester.ExpectTotalCount("Session.ZeroWindowDuration", 2);
  histogram_tester.ExpectTotalCount("Session.ZeroWindowDuration.WithUpdate",
                                    TestUpdateAvailable() ? 1 : 0);
  if (TestUpdateAvailable()) {
    histogram_tester.ExpectUniqueSample(
        "Session.ZeroWindowDuration.RestartabilityV2.Under1Min",
        RestartabilityState::SmartRestartStateFactor::kTotalBrowserCountZero,
        1);
  }
}
#endif

IN_PROC_BROWSER_TEST_P(SmartRestartMetricsObserverBrowserTest,
                       RecordsLockedDuration) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount("Session.LockedDuration", 0);

  // Simulate an upgrade.
  if (TestUpdateAvailable()) {
    fake_upgrade_detector_.SetUpgradeAvailableToRegular();
  }

  // Simulate Lock
  observer_->SetLockedStateForTesting(true);

  // Simulate Unlock
  observer_->SetLockedStateForTesting(false);

  // Verify histogram is recorded.
  histogram_tester.ExpectTotalCount("Session.LockedDuration", 1);
  histogram_tester.ExpectTotalCount("Session.LockedDuration.WithUpdate",
                                    TestUpdateAvailable() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_F(SmartRestartMetricsObserverBrowserTest,
                       RecordsLockedDuration_BeforeUnloadHandler) {
  base::HistogramTester histogram_tester;

  // Navigate to a blank page and set up the handler via script.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Set up the handler.
  ASSERT_TRUE(
      content::ExecJs(web_contents, "window.onbeforeunload = () => 'draft';"));
  // Disable hang monitor and trigger user activation.
  PrepContentsForBeforeUnloadTest(web_contents,
                                  /*trigger_user_activation=*/true);

  // Wait for the activation and handler to propagate to the browser process.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return web_contents->GetPrimaryMainFrame()
        ->CouldDisplayBeforeUnloadDialog();
  }));

  // Simulate an upgrade.
  fake_upgrade_detector_.SetUpgradeAvailableToRegular();

  // Simulate locking the screen.
  observer_->SetLockedStateForTesting(true);

  // Simulate unlocking the screen.
  observer_->SetLockedStateForTesting(false);

  // Verify the Restartability mask recorded the kBeforeUnloadHandler bit.
  histogram_tester.ExpectUniqueSample(
      "Session.LockedDuration.RestartabilityV2.Under1Min",
      RestartabilityState::SmartRestartStateFactor::kBeforeUnloadHandler, 1);

  // Cleanup: Remove the handler to allow peaceful navigation away.
  ASSERT_TRUE(content::ExecJs(web_contents, "window.onbeforeunload = null;"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(SmartRestartMetricsObserverBrowserTest,
                       RecordsLockedDuration_ZeroBrowserCount) {
  base::HistogramTester histogram_tester;

  // Close the default browser window the test starts with.
  CloseBrowserSynchronously(browser());

  // Simulate an upgrade.
  fake_upgrade_detector_.SetUpgradeAvailableToRegular();

  // Simulate locking the screen.
  observer_->SetLockedStateForTesting(true);

  // Simulate unlocking the screen.
  observer_->SetLockedStateForTesting(false);

  // Verify the Restartability mask recorded the kTotalBrowserCountZero bit.
  histogram_tester.ExpectUniqueSample(
      "Session.LockedDuration.RestartabilityV2.Under1Min",
      RestartabilityState::SmartRestartStateFactor::kTotalBrowserCountZero, 1);
}

}  // namespace smart_restart
