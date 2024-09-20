// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;

std::string GetHistogramNameWithBatteryStateSuffix(const char* histogram_name) {
  const char* suffix = base::PowerMonitor::GetInstance()->IsOnBatteryPower()
                           ? ".OnBattery"
                           : ".PluggedIn";

  return base::StrCat({histogram_name, suffix});
}

class TestTabStatsObserver : public TabStatsObserver {
 public:
  // Functions used to update the counts.
  void OnPrimaryMainFrameNavigationCommitted(
      content::WebContents* web_contents) override {
    ++main_frame_committed_navigations_count_;
  }

  size_t main_frame_committed_navigations_count() {
    return main_frame_committed_navigations_count_;
  }

 private:
  size_t main_frame_committed_navigations_count_ = 0;
};

class TestTabStatsTracker : public TabStatsTracker {
 public:
  using TabStatsTracker::OnHeartbeatEvent;
  using TabStatsTracker::OnInitialOrInsertedTab;
  using TabStatsTracker::TabChangedAt;
  using UmaStatsReportingDelegate = TabStatsTracker::UmaStatsReportingDelegate;

  explicit TestTabStatsTracker(PrefService* pref_service);

  TestTabStatsTracker(const TestTabStatsTracker&) = delete;
  TestTabStatsTracker& operator=(const TestTabStatsTracker&) = delete;

  ~TestTabStatsTracker() override {}

  // Helper functions to update the number of tabs/windows.

  size_t AddTabs(size_t tab_count,
                 ChromeRenderViewHostTestHarness* test_harness,
                 TabStripModel* tab_strip_model) {
    EXPECT_TRUE(test_harness);
    for (size_t i = 0; i < tab_count; ++i) {
      std::unique_ptr<content::WebContents> tab =
          test_harness->CreateTestWebContents();
      tab_strip_model->InsertWebContentsAt(
          tab_strip_model->count(), std::move(tab), AddTabTypes::ADD_ACTIVE);
    }
    EXPECT_EQ(tab_stats_data_store()->tab_stats().total_tab_count,
              static_cast<size_t>(tab_strip_model->count()));
    return tab_stats_data_store()->tab_stats().total_tab_count;
  }

  size_t RemoveTabs(size_t tab_count, TabStripModel* tab_strip_model) {
    EXPECT_LE(tab_count, tab_stats_data_store()->tab_stats().total_tab_count);
    EXPECT_LE(tab_count, static_cast<size_t>(tab_strip_model->count()));
    for (size_t i = 0; i < tab_count; ++i) {
      tab_strip_model->CloseWebContentsAt(tab_strip_model->count() - 1,
                                          TabCloseTypes::CLOSE_USER_GESTURE);
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

  void DiscardedStateChange(ChromeRenderViewHostTestHarness* test_harness,
                            ::mojom::LifecycleUnitDiscardReason reason,
                            bool is_discarded) {
    std::unique_ptr<content::WebContents> tab =
        test_harness->CreateTestWebContents();
    OnDiscardedStateChange(tab.get(), reason, is_discarded);
  }

  void CheckDailyEventInterval() { daily_event_for_testing()->CheckInterval(); }

  void TriggerDailyEvent() {
    // Reset the daily event to allow triggering the DailyEvent::OnInterval
    // manually several times in the same test.
    reset_daily_event_for_testing(
        new DailyEvent(pref_service_, prefs::kTabStatsDailySample,
                       /* histogram_name=*/std::string()));
    daily_event_for_testing()->AddObserver(
        std::make_unique<TabStatsDailyObserver>(
            reporting_delegate_for_testing(), tab_stats_data_store()));

    // Update the daily event registry to the previous day and trigger it.
    base::Time last_time = base::Time::Now() - base::Hours(25);
    pref_service_->SetInt64(prefs::kTabStatsDailySample,
                            last_time.since_origin().InMicroseconds());
    CheckDailyEventInterval();

    // The daily event registry should have been updated.
    EXPECT_NE(last_time.since_origin().InMicroseconds(),
              pref_service_->GetInt64(prefs::kTabStatsDailySample));
  }

  TabStatsDataStore* data_store() { return tab_stats_data_store(); }

 private:
  raw_ptr<PrefService> pref_service_;
};

class TestUmaStatsReportingDelegate
    : public TestTabStatsTracker::UmaStatsReportingDelegate {
 public:
  TestUmaStatsReportingDelegate() {}

  TestUmaStatsReportingDelegate(const TestUmaStatsReportingDelegate&) = delete;
  TestUmaStatsReportingDelegate& operator=(
      const TestUmaStatsReportingDelegate&) = delete;

 protected:
  // Skip the check that ensures that there's at least one visible window as
  // there's no window in the context of these tests.
  bool IsChromeBackgroundedWithoutWindows() override { return false; }
};

class TabStatsTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  using UmaStatsReportingDelegate =
      TestTabStatsTracker::UmaStatsReportingDelegate;

  TabStatsTrackerTest() {
    TabStatsTracker::RegisterPrefs(pref_service_.registry());

    // The tab stats tracker has to be created after the power monitor as it's
    // using it.
    tab_stats_tracker_ = std::make_unique<TestTabStatsTracker>(&pref_service_);
  }

  TabStatsTrackerTest(const TabStatsTrackerTest&) = delete;
  TabStatsTrackerTest& operator=(const TabStatsTrackerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    browser_ = CreateBrowserWithTestWindowForParams(
        Browser::CreateParams(profile(), true));
    tab_strip_model_ = browser_->tab_strip_model();
  }

  void TearDown() override {
    tab_stats_tracker_->RemoveTabs(tab_strip_model_->count(), tab_strip_model_);

    tab_stats_tracker_.reset(nullptr);
    tab_strip_model_ = nullptr;
    browser_.reset(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // The tabs stat tracker instance, it should be created in the SetUp
  std::unique_ptr<TestTabStatsTracker> tab_stats_tracker_;

  // Used to simulate power events.
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;

  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<Browser> browser_;
  raw_ptr<TabStripModel> tab_strip_model_;
};

TestTabStatsTracker::TestTabStatsTracker(PrefService* pref_service)
    : TabStatsTracker(pref_service), pref_service_(pref_service) {
  // Stop the timer to ensure that the stats don't get reported (and reset)
  // while running the tests.
  EXPECT_TRUE(daily_event_timer_for_testing()->IsRunning());
  daily_event_timer_for_testing()->Stop();

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

TEST_F(TabStatsTrackerTest, MainFrameCommittedNavigationTriggersUpdate) {
  constexpr const char kFirstUrl[] = "https://parent.com/";

  TestTabStatsObserver tab_stats_observer;
  tab_stats_tracker_->AddObserverAndSetInitialState(&tab_stats_observer);
  // Number of navigations starts of at zero.
  ASSERT_EQ(tab_stats_observer.main_frame_committed_navigations_count(), 0u);

  // Insert a new tab.
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  tab_stats_tracker_->OnInitialOrInsertedTab(web_contents.get());

  // Commit a main frame navigation on the observed tab.
  auto* parent = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL(kFirstUrl));
  DCHECK(parent);

  // Navigation registered.
  ASSERT_EQ(tab_stats_observer.main_frame_committed_navigations_count(), 1u);
}

TEST_F(TabStatsTrackerTest, OnResume) {
  // Makes sure that there's no sample initially.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName, 0);

  // Creates some tabs.
  size_t expected_tab_count =
      tab_stats_tracker_->AddTabs(12, this, tab_strip_model_);

  std::vector<base::Bucket> count_buckets;
  count_buckets.emplace_back(base::Bucket(expected_tab_count, 1));

  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);

  // Generates a resume event that should end up calling the
  // |ReportTabCountOnResume| method of the reporting delegate.
  power_monitor_source_.GenerateSuspendEvent();
  power_monitor_source_.GenerateResumeEvent();

  // There should be only one sample for the |kNumberOfTabsOnResume| histogram.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      count_buckets.size());
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
      count_buckets.size());
  EXPECT_EQ(histogram_tester_.GetAllSamples(
                UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
            count_buckets);
  EXPECT_EQ(
      histogram_tester_.GetAllSamples(GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName)),
      count_buckets);

  // Removes some tabs and update the expectations.
  expected_tab_count = tab_stats_tracker_->RemoveTabs(5, tab_strip_model_);
  count_buckets.emplace_back(base::Bucket(expected_tab_count, 1));
  std::sort(count_buckets.begin(), count_buckets.end(), CompareHistogramBucket);

  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  // Generates another resume event.
  power_monitor_source_.GenerateSuspendEvent();
  power_monitor_source_.GenerateResumeEvent();

  // There should be 2 samples for this metric now.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      count_buckets.size());
  histogram_tester_.ExpectTotalCount(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
      1);
  EXPECT_EQ(histogram_tester_.GetAllSamples(
                UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
            count_buckets);
  histogram_tester_.ExpectUniqueSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
      expected_tab_count, 1);
}

TEST_F(TabStatsTrackerTest, StatsGetReportedDaily) {
  // This test ensures that the stats get reported accurately when the daily
  // event triggers.

  // Adds some tabs and windows, then remove some so the maximums are not equal
  // to the current state.
  size_t expected_tab_count =
      tab_stats_tracker_->AddTabs(12, this, tab_strip_model_);
  size_t expected_window_count = tab_stats_tracker_->AddWindows(5);
  size_t expected_max_tab_per_window = expected_tab_count;
  tab_stats_tracker_->data_store()->UpdateMaxTabsPerWindowIfNeeded(
      expected_max_tab_per_window);
  expected_tab_count = tab_stats_tracker_->RemoveTabs(5, tab_strip_model_);
  expected_window_count = tab_stats_tracker_->RemoveWindows(2);
  expected_max_tab_per_window = expected_tab_count;

  TabsStats stats = tab_stats_tracker_->data_store()->tab_stats();

  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);
  // Trigger the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // Ensures that the histograms have been properly updated.
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kMaxTabsInADayHistogramName,
      stats.total_tab_count_max, 1);
  histogram_tester_.ExpectUniqueSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsInADayHistogramName),
      stats.total_tab_count_max, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName,
      stats.max_tab_per_window, 1);
  histogram_tester_.ExpectUniqueSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName),
      stats.max_tab_per_window, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName,
      stats.window_count_max, 1);
  histogram_tester_.ExpectUniqueSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName),
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

  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  // Trigger the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // The values in the histograms should now be equal to the current state.
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kMaxTabsInADayHistogramName,
      stats.total_tab_count_max, 1);
  histogram_tester_.ExpectBucketCount(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsInADayHistogramName),
      stats.total_tab_count_max, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName,
      stats.max_tab_per_window, 1);
  histogram_tester_.ExpectBucketCount(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName),
      stats.max_tab_per_window, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName,
      stats.window_count_max, 1);
  histogram_tester_.ExpectBucketCount(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName),
      stats.window_count_max, 1);
}

TEST_F(TabStatsTrackerTest, DailyDiscards) {
  // This test checks that the discard/reload counts are reported when the
  // daily event triggers.

  // Daily report is skipped when there is no tab. Adds tabs to avoid that.
  tab_stats_tracker_->AddTabs(1, this, tab_strip_model_);

  constexpr size_t kExpectedDiscardsExternal = 3;
  constexpr size_t kExpectedDiscardsUrgent = 5;
  constexpr size_t kExpectedDiscardsProactive = 11;
  constexpr size_t kExpectedDiscardsSuggested = 8;
  constexpr size_t kExpectedReloadsExternal = 7;
  constexpr size_t kExpectedReloadsUrgent = 9;
  constexpr size_t kExpectedReloadsProactive = 10;
  constexpr size_t kExpectedReloadsSuggested = 6;
  for (size_t i = 0; i < kExpectedDiscardsExternal; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::EXTERNAL, /*is_discarded*/ true);
  }
  for (size_t i = 0; i < kExpectedDiscardsUrgent; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::URGENT, /*is_discarded*/ true);
  }
  for (size_t i = 0; i < kExpectedDiscardsProactive; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::PROACTIVE, /*is_discarded*/ true);
  }
  for (size_t i = 0; i < kExpectedDiscardsSuggested; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::SUGGESTED, /*is_discarded*/ true);
  }
  for (size_t i = 0; i < kExpectedReloadsExternal; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::EXTERNAL, /*is_discarded*/ false);
  }
  for (size_t i = 0; i < kExpectedReloadsUrgent; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::URGENT, /*is_discarded*/ false);
  }
  for (size_t i = 0; i < kExpectedReloadsProactive; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::PROACTIVE, /*is_discarded*/ false);
  }
  for (size_t i = 0; i < kExpectedReloadsSuggested; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::SUGGESTED, /*is_discarded*/ false);
  }

  // Triggers the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // Checks that the histograms have been properly updated.
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyDiscardsExternalHistogramName,
      kExpectedDiscardsExternal, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyDiscardsUrgentHistogramName,
      kExpectedDiscardsUrgent, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyDiscardsProactiveHistogramName,
      kExpectedDiscardsProactive, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyDiscardsSuggestedHistogramName,
      kExpectedDiscardsSuggested, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyReloadsExternalHistogramName,
      kExpectedReloadsExternal, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyReloadsUrgentHistogramName,
      kExpectedReloadsUrgent, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyReloadsProactiveHistogramName,
      kExpectedReloadsProactive, 1);
  histogram_tester_.ExpectUniqueSample(
      UmaStatsReportingDelegate::kDailyReloadsSuggestedHistogramName,
      kExpectedReloadsSuggested, 1);

  // Checks that the second report also updates the histograms properly.
  constexpr size_t kExpectedDiscardsExternal2 = 15;
  constexpr size_t kExpectedDiscardsUrgent2 = 25;
  constexpr size_t kExpectedDiscardsProactive2 = 55;
  constexpr size_t kExpectedDiscardsSuggested2 = 70;
  constexpr size_t kExpectedReloadsExternal2 = 35;
  constexpr size_t kExpectedReloadsUrgent2 = 45;
  constexpr size_t kExpectedReloadsProactive2 = 40;
  constexpr size_t kExpectedReloadsSuggested2 = 27;
  for (size_t i = 0; i < kExpectedDiscardsExternal2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::EXTERNAL, /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedDiscardsUrgent2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::URGENT, /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedDiscardsProactive2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::PROACTIVE, /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedDiscardsSuggested2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::SUGGESTED, /*is_discarded=*/true);
  }
  for (size_t i = 0; i < kExpectedReloadsExternal2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::EXTERNAL, /*is_discarded=*/false);
  }
  for (size_t i = 0; i < kExpectedReloadsUrgent2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::URGENT, /*is_discarded=*/false);
  }
  for (size_t i = 0; i < kExpectedReloadsProactive2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::PROACTIVE, /*is_discarded=*/false);
  }
  for (size_t i = 0; i < kExpectedReloadsSuggested2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::SUGGESTED, /*is_discarded=*/false);
  }

  // Triggers the daily event again.
  tab_stats_tracker_->TriggerDailyEvent();

  // Checks that the histograms have been properly updated.
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyDiscardsExternalHistogramName,
      kExpectedDiscardsExternal2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyDiscardsUrgentHistogramName,
      kExpectedDiscardsUrgent2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyDiscardsProactiveHistogramName,
      kExpectedDiscardsProactive2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyDiscardsSuggestedHistogramName,
      kExpectedDiscardsSuggested2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyReloadsExternalHistogramName,
      kExpectedReloadsExternal2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyReloadsUrgentHistogramName,
      kExpectedReloadsUrgent2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyReloadsProactiveHistogramName,
      kExpectedReloadsProactive2, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kDailyReloadsSuggestedHistogramName,
      kExpectedReloadsSuggested2, 1);
}

TEST_F(TabStatsTrackerTest, HeartbeatMetrics) {
  size_t expected_tab_count =
      tab_stats_tracker_->AddTabs(12, this, tab_strip_model_);
  size_t expected_window_count = tab_stats_tracker_->AddWindows(5);

  tab_stats_tracker_->OnHeartbeatEvent();

  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kTabCountHistogramName, expected_tab_count, 1);
  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kWindowCountHistogramName,
      expected_window_count, 1);

  expected_tab_count = tab_stats_tracker_->RemoveTabs(4, tab_strip_model_);
  expected_window_count = tab_stats_tracker_->RemoveWindows(3);

  ASSERT_TRUE(tab_strip_model_->SupportsTabGroups());
  tab_groups::TabGroupId group_id1 = tab_strip_model_->AddToNewGroup({0, 1});
  tab_groups::TabGroupId group_id2 = tab_strip_model_->AddToNewGroup({5});
  const tab_groups::TabGroupVisualData visual_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan, /* is_collapsed = */ true);
  TabGroup* group1 = tab_strip_model_->group_model()->GetTabGroup(group_id1);
  TabGroup* group2 = tab_strip_model_->group_model()->GetTabGroup(group_id2);
  group1->SetVisualData(visual_data);
  group2->SetVisualData(visual_data);
  ASSERT_TRUE(tab_strip_model_->IsGroupCollapsed(group_id1));
  ASSERT_TRUE(tab_strip_model_->IsGroupCollapsed(group_id2));

  tab_stats_tracker_->OnHeartbeatEvent();

  histogram_tester_.ExpectBucketCount(
      UmaStatsReportingDelegate::kWindowCountHistogramName,
      expected_window_count, 1);
}

}  // namespace metrics
