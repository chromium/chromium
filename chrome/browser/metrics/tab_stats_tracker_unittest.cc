// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_tracker.h"

#include <algorithm>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;

class TestTabStatsTracker : public TabStatsTracker {
 public:
  using TabStatsTracker::OnInitialOrInsertedTab;
  using TabStatsTracker::OnInterval;
  using TabStatsTracker::OnHeartbeatEvent;
  using TabStatsTracker::TabChangedAt;
  using UmaStatsReportingDelegate = TabStatsTracker::UmaStatsReportingDelegate;

  explicit TestTabStatsTracker(PrefService* pref_service);
  ~TestTabStatsTracker() override {}

  // Helper functions to update the number of tabs/windows.

  size_t AddTabs(size_t tab_count,
                 ChromeRenderViewHostTestHarness* test_harness) {
    EXPECT_TRUE(test_harness);
    for (size_t i = 0; i < tab_count; ++i) {
      std::unique_ptr<content::WebContents> tab =
          test_harness->CreateTestWebContents();
      tab_stats_data_store()->OnTabAdded(tab.get());
      tabs_.emplace_back(std::move(tab));
    }
    return tab_stats_data_store()->tab_stats().total_tab_count;
  }

  size_t RemoveTabs(size_t tab_count) {
    EXPECT_LE(tab_count, tab_stats_data_store()->tab_stats().total_tab_count);
    EXPECT_LE(tab_count, tabs_.size());
    for (size_t i = 0; i < tab_count; ++i) {
      tab_stats_data_store()->OnTabRemoved(tabs_.back().get());
      tabs_.pop_back();
    }
    return tab_stats_data_store()->tab_stats().total_tab_count;
  }

  size_t AddWindows(size_t window_count) {
    for (size_t i = 0; i < window_count; ++i)
      tab_stats_data_store()->OnWindowAdded();
    return tab_stats_data_store()->tab_stats().window_count;
  }

  size_t RemoveWindows(size_t window_count) {
    EXPECT_LE(window_count, tab_stats_data_store()->tab_stats().window_count);
    for (size_t i = 0; i < window_count; ++i)
      tab_stats_data_store()->OnWindowRemoved();
    return tab_stats_data_store()->tab_stats().window_count;
  }

  void CheckDailyEventInterval() { daily_event_for_testing()->CheckInterval(); }

  void TriggerDailyEvent() {
    // Reset the daily event to allow triggering the DailyEvent::OnInterval
    // manually several times in the same test.
    reset_daily_event_for_testing(
        new DailyEvent(pref_service_, prefs::kTabStatsDailySample,
                       kTabStatsDailyEventHistogramName));
    daily_event_for_testing()->AddObserver(
        std::make_unique<TabStatsDailyObserver>(
            reporting_delegate_for_testing(), tab_stats_data_store()));

    // Update the daily event registry to the previous day and trigger it.
    base::Time last_time = base::Time::Now() - base::TimeDelta::FromHours(25);
    pref_service_->SetInt64(prefs::kTabStatsDailySample,
                            last_time.since_origin().InMicroseconds());
    CheckDailyEventInterval();

    // The daily event registry should have been updated.
    EXPECT_NE(last_time.since_origin().InMicroseconds(),
              pref_service_->GetInt64(prefs::kTabStatsDailySample));
  }

  TabStatsDataStore* data_store() { return tab_stats_data_store(); }

 private:
  PrefService* pref_service_;

  std::vector<std::unique_ptr<content::WebContents>> tabs_;

  DISALLOW_COPY_AND_ASSIGN(TestTabStatsTracker);
};

class TestUmaStatsReportingDelegate
    : public TestTabStatsTracker::UmaStatsReportingDelegate {
 public:
  using TestTabStatsTracker::UmaStatsReportingDelegate::
      GetIntervalHistogramName;
  TestUmaStatsReportingDelegate() {}

 protected:
  // Skip the check that ensures that there's at least one visible window as
  // there's no window in the context of these tests.
  bool IsChromeBackgroundedWithoutWindows() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestUmaStatsReportingDelegate);
};

class TabStatsTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  using UmaStatsReportingDelegate =
      TestTabStatsTracker::UmaStatsReportingDelegate;

  TabStatsTrackerTest() {
    power_monitor_source_ = new base::PowerMonitorTestSource();
    base::PowerMonitor::Initialize(
        std::unique_ptr<base::PowerMonitorSource>(power_monitor_source_));

    TabStatsTracker::RegisterPrefs(pref_service_.registry());

    // The tab stats tracker has to be created after the power monitor as it's
    // using it.
    tab_stats_tracker_.reset(new TestTabStatsTracker(&pref_service_));
  }

  void TearDown() override {
    tab_stats_tracker_.reset(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
    base::PowerMonitor::ShutdownForTesting();
  }

  // The tabs stat tracker instance, it should be created in the SetUp
  std::unique_ptr<TestTabStatsTracker> tab_stats_tracker_;

  // Used to simulate power events.
  base::PowerMonitorTestSource* power_monitor_source_;

  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

  TestingPrefServiceSimple pref_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabStatsTrackerTest);
};

TestTabStatsTracker::TestTabStatsTracker(PrefService* pref_service)
    : TabStatsTracker(pref_service), pref_service_(pref_service) {
  // Stop the timer to ensure that the stats don't get reported (and reset)
  // while running the tests.
  EXPECT_TRUE(daily_event_timer_for_testing()->IsRunning());
  daily_event_timer_for_testing()->Stop();

  // Stop the usage interval timers so they don't trigger while running the
  // tests.
  usage_interval_timers_for_testing()->clear();

  reset_reporting_delegate_for_testing(new TestUmaStatsReportingDelegate());

  // Stop the heartbeat timer to ensure that it doesn't interfere with the
  // tests.
  heartbeat_timer_for_testing()->Stop();
}

// Comparator for base::Bucket values.
bool CompareHistogramBucket(const base::Bucket& l, const base::Bucket& r) {
  return l.min < r.min;
}

}  // namespace

TEST_F(TabStatsTrackerTest, OnResume) {
  // Makes sure that there's no sample initially.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName, 0);

  // Creates some tabs.
  size_t expected_tab_count = tab_stats_tracker_->AddTabs(12, this);

  std::vector<base::Bucket> count_buckets;
  count_buckets.emplace_back(base::Bucket(expected_tab_count, 1));

  // Generates a resume event that should end up calling the
  // |ReportTabCountOnResume| method of the reporting delegate.
  power_monitor_source_->GenerateSuspendEvent();
  power_monitor_source_->GenerateResumeEvent();

  // There should be only one sample for the |kNumberOfTabsOnResume| histogram.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      count_buckets.size());
  EXPECT_EQ(histogram_tester_.GetAllSamples(
                UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
            count_buckets);

  // Removes some tabs and update the expectations.
  expected_tab_count = tab_stats_tracker_->RemoveTabs(5);
  count_buckets.emplace_back(base::Bucket(expected_tab_count, 1));
  std::sort(count_buckets.begin(), count_buckets.end(), CompareHistogramBucket);

  // Generates another resume event.
  power_monitor_source_->GenerateSuspendEvent();
  power_monitor_source_->GenerateResumeEvent();

  // There should be 2 samples for this metric now.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      count_buckets.size());
  EXPECT_EQ(histogram_tester_.GetAllSamples(
                UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
            count_buckets);
}

TEST_F(TabStatsTrackerTest, StatsGetReportedDaily) {
  // This test ensures that the stats get reported accurately when the daily
  // event triggers.

  // Adds some tabs and windows, then remove some so the maximums are not equal
  // to the current state.
  size_t expected_tab_count = tab_stats_tracker_->AddTabs(12, this);
  size_t expected_window_count = tab_stats_tracker_->AddWindows(5);
  size_t expected_max_tab_per_window = expected_tab_count - 1;
  tab_stats_tracker_->data_store()->UpdateMaxTabsPerWindowIfNeeded(
      expected_max_tab_per_window);
  expected_tab_count = tab_stats_tracker_->RemoveTabs(5);
  expected_window_count = tab_stats_tracker_->RemoveWindows(2);
  expected_max_tab_per_window = expected_tab_count - 1;

  TabsStats stats = tab_stats_tracker_->data_store()->tab_stats();

  // Trigger the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // Ensures that the histograms have been properly updated.
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kMaxTabsInADayHistogramName,
      stats.total_tab_count_max, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName,
      stats.max_tab_per_window, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName,
      stats.window_count_max, 1);

  // Manually call the function to update the maximum number of tabs in a single
  // window. This is normally done automatically in the reporting function by
  // scanning the list of existing windows, but doesn't work here as this isn't
  // a window test.
  tab_stats_tracker_->data_store()->UpdateMaxTabsPerWindowIfNeeded(
      expected_max_tab_per_window);

  // Make sure that the maximum values have been updated to the current state.
  stats = tab_stats_tracker_->data_store()->tab_stats();
  EXPECT_EQ(expected_tab_count, stats.total_tab_count_max);
  EXPECT_EQ(expected_max_tab_per_window, stats.max_tab_per_window);
  EXPECT_EQ(expected_window_count, stats.window_count_max);
  EXPECT_EQ(expected_tab_count, static_cast<size_t>(pref_service_.GetInteger(
                                    prefs::kTabStatsTotalTabCountMax)));
  EXPECT_EQ(expected_max_tab_per_window,
            static_cast<size_t>(
                pref_service_.GetInteger(prefs::kTabStatsMaxTabsPerWindow)));
  EXPECT_EQ(expected_window_count, static_cast<size_t>(pref_service_.GetInteger(
                                       prefs::kTabStatsWindowCountMax)));

  // Trigger the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // The values in the histograms should now be equal to the current state.
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kMaxTabsInADayHistogramName,
      stats.total_tab_count_max, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName,
      stats.max_tab_per_window, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName,
      stats.window_count_max, 1);
}

TEST_F(TabStatsTrackerTest, TabUsageGetsReported) {
  constexpr base::TimeDelta kValidLongInterval = base::TimeDelta::FromHours(12);
  TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
      tab_stats_tracker_->data_store()->AddInterval();

  std::vector<std::unique_ptr<content::WebContents>> web_contentses;
  for (size_t i = 0; i < 4; ++i) {
    web_contentses.emplace_back(CreateTestWebContents());
    // Make sure that these WebContents are initially not visible.
    web_contentses[i]->WasHidden();
    tab_stats_tracker_->OnInitialOrInsertedTab(web_contentses[i].get());
  }

  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);

  histogram_tester_.ExpectUniqueSample(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::
              kUnusedAndClosedInIntervalHistogramNameBase,
          kValidLongInterval),
      0, 1);
  histogram_tester_.ExpectUniqueSample(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUnusedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      web_contentses.size(), 1);
  histogram_tester_.ExpectUniqueSample(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedAndClosedInIntervalHistogramNameBase,
          kValidLongInterval),
      0, 1);
  histogram_tester_.ExpectUniqueSample(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      0, 1);

  // Mark one tab as visible and make sure that it get reported properly.
  web_contentses[0]->WasShown();
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::
              kUnusedAndClosedInIntervalHistogramNameBase,
          kValidLongInterval),
      0, 2);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUnusedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      web_contentses.size() - 1, 1);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedAndClosedInIntervalHistogramNameBase,
          kValidLongInterval),
      0, 2);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      1, 1);

  // Mark a tab as audible and make sure that we now have 2 tabs marked as used.
  content::WebContentsTester::For(web_contentses[1].get())
      ->SetIsCurrentlyAudible(true);
  tab_stats_tracker_->TabChangedAt(web_contentses[1].get(), 1,
                                   TabChangeType::kAll);
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUnusedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      web_contentses.size() - 2, 1);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      2, 1);

  // Simulate an interaction on a tab, we should now see 3 tabs being marked as
  // used.
  content::WebContentsTester::For(web_contentses[2].get())
      ->TestDidReceiveInputEvent(blink::WebInputEvent::kMouseDown);
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUnusedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      web_contentses.size() - 3, 1);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedTabsInIntervalHistogramNameBase,
          kValidLongInterval),
      3, 1);

  // Remove the last WebContents, which should be reported as an unused tab.
  web_contentses.pop_back();
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::
              kUnusedAndClosedInIntervalHistogramNameBase,
          kValidLongInterval),
      1, 1);

  // Remove an active WebContents and make sure that this get reported properly.
  //
  // We need to re-interact with the WebContents as each call to |OnInterval|
  // reset the interval and clear the interaction bit.
  content::WebContentsTester::For(web_contentses.back().get())
      ->TestDidReceiveInputEvent(blink::WebInputEvent::kMouseDown);
  web_contentses.pop_back();
  tab_stats_tracker_->OnInterval(kValidLongInterval, interval_map);
  histogram_tester_.ExpectBucketCount(
      TestUmaStatsReportingDelegate::GetIntervalHistogramName(
          UmaStatsReportingDelegate::kUsedAndClosedInIntervalHistogramNameBase,
          kValidLongInterval),
      1, 1);
}

TEST_F(TabStatsTrackerTest, HeartbeatMetrics) {
  size_t expected_tab_count = tab_stats_tracker_->AddTabs(12, this);
  size_t expected_window_count = tab_stats_tracker_->AddWindows(5);

  tab_stats_tracker_->OnHeartbeatEvent();

  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kTabCountHistogramName, expected_tab_count, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kWindowCountHistogramName,
      expected_window_count, 1);

  expected_tab_count = tab_stats_tracker_->RemoveTabs(4);
  expected_window_count = tab_stats_tracker_->RemoveWindows(3);

  tab_stats_tracker_->OnHeartbeatEvent();

  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kTabCountHistogramName, expected_tab_count, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kWindowCountHistogramName,
      expected_window_count, 1);
}

}  // namespace metrics
