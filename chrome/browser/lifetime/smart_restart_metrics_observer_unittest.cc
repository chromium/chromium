// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/smart_restart_metrics_observer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace smart_restart {
namespace {

// A fake UpgradeDetector to control update availability in tests.
class FakeUpgradeDetector : public UpgradeDetector {
 public:
  explicit FakeUpgradeDetector(const base::Clock* clock,
                               const base::TickClock* tick_clock)
      : UpgradeDetector(clock, tick_clock) {
    set_upgrade_detected_time(this->clock()->Now());
  }

  FakeUpgradeDetector(const FakeUpgradeDetector&) = delete;
  FakeUpgradeDetector& operator=(const FakeUpgradeDetector&) = delete;

  // UpgradeDetector overrides:
  base::Time GetAnnoyanceLevelDeadline(
      UpgradeNotificationAnnoyanceLevel level) override {
    return base::Time();
  }

  void SetUpgradeAvailableToRegular() {
    set_upgrade_available(UPGRADE_AVAILABLE_REGULAR);
  }
};

}  // namespace

class SmartRestartMetricsObserverTest : public testing::Test {
 public:
  SmartRestartMetricsObserverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        upgrade_detector_(task_environment_.GetMockClock(),
                          task_environment_.GetMockTickClock()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    observer_ = std::make_unique<SmartRestartMetricsObserver>(
        &upgrade_detector_,
        base::BindRepeating(
            &SmartRestartMetricsObserverTest::IsZeroBrowserCallback,
            base::Unretained(this)));
  }

  void TearDown() override { observer_.reset(); }

  FakeUpgradeDetector* upgrade_detector() { return &upgrade_detector_; }

  bool IsZeroBrowserCallback() { return browser_count_ == 0; }

  void set_browser_count(size_t count) { browser_count_ = count; }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeUpgradeDetector upgrade_detector_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<SmartRestartMetricsObserver> observer_;
  size_t browser_count_ = 1;
};

#if BUILDFLAG(IS_MAC)
TEST_F(SmartRestartMetricsObserverTest, RecordZeroWindowDuration) {
  base::HistogramTester histogram_tester;

  // 1. Initially we have a browser.
  set_browser_count(1);

  // 2. Simulate entering "Zero Window" state.
  set_browser_count(0);
  observer_->OnBrowserClosed(nullptr);

  // 3. Advance time to simulate a 1-minute background duration.
  task_environment_.FastForwardBy(base::Minutes(1));

  // 4. End the "Zero Window" state.
  set_browser_count(1);
  observer_->OnBrowserCreated(nullptr);

  // 5. Verify that the duration was recorded correctly.
  histogram_tester.ExpectUniqueTimeSample("Session.ZeroWindowDuration",
                                          base::Minutes(1), 1);
  histogram_tester.ExpectTotalCount("Session.ZeroWindowDuration.WithUpdate", 0);
}

TEST_F(SmartRestartMetricsObserverTest, RecordZeroWindowDurationWithUpdate) {
  base::HistogramTester histogram_tester;

  // 1. Simulate a pending update.
  upgrade_detector()->SetUpgradeAvailableToRegular();

  // 2. Enter Zero Window state.
  set_browser_count(0);
  observer_->OnBrowserClosed(nullptr);

  // 3. Advance time.
  task_environment_.FastForwardBy(base::Minutes(5));

  // 4. Exit Zero Window state.
  set_browser_count(1);
  observer_->OnBrowserCreated(nullptr);

  // 5. Verify both metrics.
  histogram_tester.ExpectUniqueTimeSample("Session.ZeroWindowDuration",
                                          base::Minutes(5), 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Session.ZeroWindowDuration.WithUpdate", base::Minutes(5), 1);
  histogram_tester.ExpectUniqueSample(
      "Session.ZeroWindowDuration.RestartabilityV2.5To10Min",
      32 /* kTotalBrowserCountZero */, 1);
}

TEST_F(SmartRestartMetricsObserverTest, NoRecordIfNotEmpty) {
  base::HistogramTester histogram_tester;

  // 1. Have 2 browsers.
  set_browser_count(2);

  // 2. Remove one. Count is 1, not 0.
  set_browser_count(1);
  observer_->OnBrowserClosed(nullptr);

  task_environment_.FastForwardBy(base::Minutes(1));

  // 3. Remove the final browser.
  set_browser_count(0);
  observer_->OnBrowserClosed(nullptr);

  // 4. Advance time for 2 minutes.
  task_environment_.FastForwardBy(base::Minutes(2));

  // 5. Re-add browser.
  set_browser_count(1);
  observer_->OnBrowserCreated(nullptr);

  // 6. Only the 2-minute duration should be recorded.
  histogram_tester.ExpectUniqueTimeSample("Session.ZeroWindowDuration",
                                          base::Minutes(2), 1);
}

TEST_F(SmartRestartMetricsObserverTest, RecordOnDestruction) {
  base::HistogramTester histogram_tester;

  // 1. Enter Zero Window state.
  set_browser_count(0);
  observer_->OnBrowserClosed(nullptr);

  // 2. Advance time.
  task_environment_.FastForwardBy(base::Minutes(10));

  // 3. Destroy observer (simulates app quit).
  observer_.reset();

  // 4. Verify metric was recorded.
  histogram_tester.ExpectUniqueTimeSample("Session.ZeroWindowDuration",
                                          base::Minutes(10), 1);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(SmartRestartMetricsObserverTest, RecordLockedDuration) {
  base::HistogramTester histogram_tester;

  // 1. Transition to LOCKED.
  observer_->SetLockedStateForTesting(true);

  // 2. Advance time for 10 minutes.
  task_environment_.FastForwardBy(base::Minutes(10));

  // 3. Transition to UNLOCKED.
  observer_->SetLockedStateForTesting(false);

  // 4. Verify metric.
  histogram_tester.ExpectUniqueTimeSample("Session.LockedDuration",
                                          base::Minutes(10), 1);
  histogram_tester.ExpectTotalCount("Session.LockedDuration.WithUpdate", 0);
}

TEST_F(SmartRestartMetricsObserverTest, RecordLockedDurationWithUpdate) {
  base::HistogramTester histogram_tester;

  // 1. Set update available.
  upgrade_detector()->SetUpgradeAvailableToRegular();

  // 2. Transition to LOCKED.
  observer_->SetLockedStateForTesting(true);

  // 3. Advance time.
  task_environment_.FastForwardBy(base::Minutes(30));

  // 4. Transition to UNLOCKED.
  observer_->SetLockedStateForTesting(false);

  // 5. Verify both metrics.
  histogram_tester.ExpectUniqueTimeSample("Session.LockedDuration",
                                          base::Minutes(30), 1);
  histogram_tester.ExpectUniqueTimeSample("Session.LockedDuration.WithUpdate",
                                          base::Minutes(30), 1);
  histogram_tester.ExpectUniqueSample(
      "Session.LockedDuration.RestartabilityV2.Over10Min",
      32 /* kTotalBrowserCountZero */, 1);
}

TEST_F(SmartRestartMetricsObserverTest, NoRecordIfNeverLocked) {
  base::HistogramTester histogram_tester;

  task_environment_.FastForwardBy(base::Minutes(5));
  histogram_tester.ExpectTotalCount("Session.LockedDuration", 0);
}

TEST_F(SmartRestartMetricsObserverTest, RecordLockOnDestruction) {
  base::HistogramTester histogram_tester;

  // 1. Transition to LOCKED.
  observer_->SetLockedStateForTesting(true);

  // 2. Advance time.
  task_environment_.FastForwardBy(base::Minutes(60));

  // 3. Destroy observer (simulates app quit while locked).
  observer_.reset();

  // 4. Verify metric was recorded.
  histogram_tester.ExpectUniqueTimeSample("Session.LockedDuration",
                                          base::Minutes(60), 1);
}

}  // namespace smart_restart
