// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_samples.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#else
#include "chrome/browser/resource_coordinator/test_lifecycle_unit.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#endif

namespace metrics {

namespace {

using TabsStats = TabStatsDataStore::TabsStats;
using TabStripInterface = TabStatsTracker::TabStripInterface;

std::string GetHistogramNameWithBatteryStateSuffix(const char* histogram_name) {
  const char* suffix = base::PowerMonitor::GetInstance()->IsOnBatteryPower()
                           ? ".OnBattery"
                           : ".PluggedIn";

  return base::StrCat({histogram_name, suffix});
}

// Like Bucket from histogram_tester.h, but includes the max value so it's
// easier to test whether samples fall inside the bucket.
struct HistogramBucket {
  int32_t min = 0;  // Inclusive
  int64_t max = 0;  // Exclusive
  int32_t count = 0;
};

std::ostream& operator<<(std::ostream& os, const HistogramBucket& bucket) {
  return os << "Bucket[" << bucket.min << "," << bucket.max
            << "):" << bucket.count;
}

// A GMock matcher that matches a HistogramBucket if `sample` is between `min`
// and `max`, and `count` matches exactly.
auto SampleCountInBucketMatcher(int sample, int count) {
  using ::testing::Field;
  using ::testing::Gt;
  using ::testing::Le;
  return ::testing::AllOf(Field("min", &HistogramBucket::min, Le(sample)),
                          Field("max", &HistogramBucket::max, Gt(sample)),
                          Field("count", &HistogramBucket::count, count));
}

class TestTabStatsObserver : public TabStatsObserver {
 public:
  explicit TestTabStatsObserver(TabStatsTracker& stats_tracker)
      : stats_tracker_(stats_tracker) {
    stats_tracker_->AddObserverAndSetInitialState(this);
  }
  ~TestTabStatsObserver() override { stats_tracker_->RemoveObserver(this); }

  // Functions used to update the counts.
  void OnPrimaryMainFrameNavigationCommitted(
      content::WebContents* web_contents) override {
    ++main_frame_committed_navigations_count_;
  }
  void OnVideoStartedPlaying(content::WebContents* web_contents) override {
    ASSERT_FALSE(video_playing_in_tab_);
    video_playing_in_tab_ = true;
  }
  void OnVideoStoppedPlaying(content::WebContents* web_contents) override {
    ASSERT_TRUE(video_playing_in_tab_);
    video_playing_in_tab_ = false;
  }

  size_t main_frame_committed_navigations_count() {
    return main_frame_committed_navigations_count_;
  }

  bool is_video_playing_in_tab() const { return video_playing_in_tab_; }

 private:
  const base::raw_ref<TabStatsTracker> stats_tracker_;
  size_t main_frame_committed_navigations_count_ = 0;
  bool video_playing_in_tab_ = false;
};

// Modifies the TabStripModel (on Desktop) or TabModel (on Android).

#if BUILDFLAG(IS_ANDROID)

class TabStripModifier {
 public:
  TabStripModifier(const TabStripInterface* tab_strip,
                   OwningTestTabModel* test_tab_model)
      : tab_strip_(tab_strip), test_tab_model_(test_tab_model) {}

  ~TabStripModifier() = default;

  TabStripModifier(const TabStripModifier&) = delete;
  TabStripModifier& operator=(const TabStripModifier&) = delete;

  const TabStripInterface& tab_strip() const { return *tab_strip_; }

  void InsertWebContentsAt(size_t index,
                           std::unique_ptr<content::WebContents> web_contents) {
    test_tab_model_->AddTabFromWebContents(std::move(web_contents), index,
                                           /*select=*/true);
  }

  void CloseWebContentsAt(size_t index) { test_tab_model_->CloseTabAt(index); }

 private:
  raw_ptr<const TabStripInterface> tab_strip_;
  raw_ptr<OwningTestTabModel> test_tab_model_;
};

#else  // !BUILDFLAG(IS_ANDROID)

class TabStripModifier {
 public:
  TabStripModifier(const TabStripInterface* tab_strip, Browser* browser)
      : tab_strip_(tab_strip), browser_(browser) {}

  const TabStripInterface& tab_strip() const { return *tab_strip_; }

  void InsertWebContentsAt(size_t index,
                           std::unique_ptr<content::WebContents> web_contents) {
    browser_->tab_strip_model()->InsertWebContentsAt(
        index, std::move(web_contents), AddTabTypes::ADD_ACTIVE);
  }

  void CloseWebContentsAt(size_t index) {
    browser_->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_USER_GESTURE);
  }

 private:
  raw_ptr<const TabStripInterface> tab_strip_;
  raw_ptr<Browser> browser_;
};

#endif  // !BUILDFLAG(IS_ANDROID)

class TestTabStatsTracker : public TabStatsTracker {
 public:
  using TabStatsTracker::OnHeartbeatEvent;
  using TabStatsTracker::OnInitialOrInsertedTab;
  using UmaStatsReportingDelegate = TabStatsTracker::UmaStatsReportingDelegate;

  explicit TestTabStatsTracker(PrefService* pref_service);

  TestTabStatsTracker(const TestTabStatsTracker&) = delete;
  TestTabStatsTracker& operator=(const TestTabStatsTracker&) = delete;

  ~TestTabStatsTracker() override = default;

  // Helper functions to update the number of tabs/windows.

  size_t AddTabs(size_t tab_count,
                 ChromeRenderViewHostTestHarness* test_harness,
                 TabStripModifier* tab_strip_modifier) {
    EXPECT_TRUE(test_harness);
    for (size_t i = 0; i < tab_count; ++i) {
      std::unique_ptr<content::WebContents> tab =
          test_harness->CreateTestWebContents();
      tab_strip_modifier->InsertWebContentsAt(
          tab_strip_modifier->tab_strip().GetTabCount(), std::move(tab));
    }
    EXPECT_EQ(tab_stats_data_store()->tab_stats().total_tab_count,
              tab_strip_modifier->tab_strip().GetTabCount());
    return tab_stats_data_store()->tab_stats().total_tab_count;
  }

  size_t RemoveTabs(size_t tab_count, TabStripModifier* tab_strip_modifier) {
    EXPECT_LE(tab_count, tab_stats_data_store()->tab_stats().total_tab_count);
    EXPECT_LE(tab_count, tab_strip_modifier->tab_strip().GetTabCount());
    for (size_t i = 0; i < tab_count; ++i) {
      tab_strip_modifier->CloseWebContentsAt(
          tab_strip_modifier->tab_strip().GetTabCount() - 1);
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

#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/412634171): Enable this when discarding is supported on
  // Android.
  void DiscardedStateChange(ChromeRenderViewHostTestHarness* test_harness,
                            ::mojom::LifecycleUnitDiscardReason reason,
                            bool is_discarded) {
    static constexpr auto kStateChangeReason =
        ::mojom::LifecycleUnitStateChangeReason::BROWSER_INITIATED;

    resource_coordinator::TestLifecycleUnit lifecycle_unit;
    lifecycle_unit.SetDiscardReason(reason);
    lifecycle_unit.SetState(is_discarded
                                ? ::mojom::LifecycleUnitState::DISCARDED
                                : ::mojom::LifecycleUnitState::ACTIVE,
                            kStateChangeReason);
    const auto previous_state = is_discarded
                                    ? ::mojom::LifecycleUnitState::ACTIVE
                                    : ::mojom::LifecycleUnitState::DISCARDED;
    OnLifecycleUnitStateChanged(&lifecycle_unit, previous_state,
                                kStateChangeReason);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

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
  TestUmaStatsReportingDelegate() = default;

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
#if BUILDFLAG(IS_ANDROID)
    test_tab_model_ = std::make_unique<OwningTestTabModel>(profile());
    test_tab_model_->AddEmptyTab(0);
    tab_strip_interface_ =
        std::make_unique<TabStripInterface>(test_tab_model_.get());
    tab_strip_modifier_ = std::make_unique<TabStripModifier>(
        tab_strip_interface_.get(), test_tab_model_.get());
#else
    browser_ = CreateBrowserWithTestWindowForParams(
        Browser::CreateParams(profile(), true));
    tab_strip_interface_ = std::make_unique<TabStripInterface>(browser_.get());
    tab_strip_modifier_ = std::make_unique<TabStripModifier>(
        tab_strip_interface_.get(), browser_.get());
#endif
  }

  void TearDown() override {
    tab_stats_tracker_->RemoveTabs(tab_strip_interface_->GetTabCount(),
                                   tab_strip_modifier_.get());
    tab_stats_tracker_.reset();

    // Everything depending on `profile()` must be destroyed before it's deleted
    // in TearDown.
    tab_strip_modifier_.reset();
    tab_strip_interface_.reset();
#if BUILDFLAG(IS_ANDROID)
    test_tab_model_.reset();
#else
    browser_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::vector<HistogramBucket> GetHistogramBuckets(
      std::string_view histogram_name) {
    auto samples =
        histogram_tester_.GetHistogramSamplesSinceCreation(histogram_name);
    auto iterator = samples->Iterator();
    std::vector<HistogramBucket> buckets;
    while (!iterator->Done()) {
      HistogramBucket bucket;
      iterator->Get(&bucket.min, &bucket.max, &bucket.count);
      buckets.push_back(bucket);
      iterator->Next();
    }
    return buckets;
  }

  // Expects that `histogram_name` has a bucket containing `sample`, with count
  // `expected_count`. There may be samples in other buckets.
  void ExpectBucketedSample(
      std::string_view histogram_name,
      int sample,
      int expected_count,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    EXPECT_THAT(GetHistogramBuckets(histogram_name),
                ::testing::Contains(
                    SampleCountInBucketMatcher(sample, expected_count)));
  }

  // Expects that `histogram_name` has exactly one bucket containing `sample`,
  // with count `expected_count`.
  void ExpectUniqueBucketedSample(
      std::string_view histogram_name,
      int sample,
      int expected_count,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    EXPECT_THAT(GetHistogramBuckets(histogram_name),
                ::testing::ElementsAre(
                    SampleCountInBucketMatcher(sample, expected_count)));
  }

  // Expects that `histogram_name` contains an exact list of buckets. Each entry
  // in `expected_sample_counts` is a sample mapped to the expected count in the
  // bucket containing that sample.
  void ExpectExactBucketedSamples(
      std::string_view histogram_name,
      const std::map<int, int>& expected_sample_counts,
      const base::Location& location = base::Location::Current()) {
    SCOPED_TRACE(location.ToString());
    std::vector<::testing::Matcher<HistogramBucket>> matchers;
    for (const auto& [sample, count] : expected_sample_counts) {
      matchers.push_back(SampleCountInBucketMatcher(sample, count));
    }
    EXPECT_THAT(GetHistogramBuckets(histogram_name),
                ::testing::UnorderedElementsAreArray(matchers));
  }

  // The tabs stat tracker instance, it should be created in the SetUp
  std::unique_ptr<TestTabStatsTracker> tab_stats_tracker_;

  // Used to simulate power events.
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;

  // Used to make sure that the metrics are reported properly.
  base::HistogramTester histogram_tester_;

  TestingPrefServiceSimple pref_service_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<OwningTestTabModel> test_tab_model_;
#else
  std::unique_ptr<Browser> browser_;
#endif

  // Wrappers for the TabStripModel on desktop or TabModel on Android.
  std::unique_ptr<TabStripInterface> tab_strip_interface_;
  std::unique_ptr<TabStripModifier> tab_strip_modifier_;
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

}  // namespace

TEST_F(TabStatsTrackerTest, MainFrameCommittedNavigationTriggersUpdate) {
  constexpr const char kFirstUrl[] = "https://parent.com/";

  TestTabStatsObserver tab_stats_observer(*tab_stats_tracker_);
  // Number of navigations starts of at zero.
  ASSERT_EQ(tab_stats_observer.main_frame_committed_navigations_count(), 0u);

  // Insert a new tab.
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  tab_stats_tracker_->OnInitialOrInsertedTab(web_contents.get());

  // Commit a main frame navigation on the observed tab.
  auto* parent = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents.get(), GURL(kFirstUrl));
  ASSERT_TRUE(parent);

  // Navigation registered.
  ASSERT_EQ(tab_stats_observer.main_frame_committed_navigations_count(), 1u);
}

TEST_F(TabStatsTrackerTest, OnResume) {
  // Makes sure that there's no sample initially.
  histogram_tester_.ExpectTotalCount(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName, 0);

  // Creates some tabs.
  size_t expected_tab_count =
      tab_stats_tracker_->AddTabs(12, this, tab_strip_modifier_.get());

  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);

  // Generates a resume event that should end up calling the
  // |ReportTabCountOnResume| method of the reporting delegate.
  power_monitor_source_.GenerateSuspendEvent();
  power_monitor_source_.GenerateResumeEvent();

  // There should be only one sample for the |kNumberOfTabsOnResume| histogram.
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      expected_tab_count, 1);
  ExpectUniqueBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
      expected_tab_count, 1);

  // Removes some tabs and update the expectations.
  size_t expected_tab_count2 =
      tab_stats_tracker_->RemoveTabs(5, tab_strip_modifier_.get());

  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  // Generates another resume event.
  power_monitor_source_.GenerateSuspendEvent();
  power_monitor_source_.GenerateResumeEvent();

  // There should be 2 samples for this metric now.
  ExpectExactBucketedSamples(
      UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName,
      {{expected_tab_count, 1}, {expected_tab_count2, 1}});
  // This metric should only contain the newer sample.
  ExpectUniqueBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kNumberOfTabsOnResumeHistogramName),
      expected_tab_count2, 1);
}

TEST_F(TabStatsTrackerTest, StatsGetReportedDaily) {
  // This test ensures that the stats get reported accurately when the daily
  // event triggers.

  // Adds some tabs and windows, then remove some so the maximums are not equal
  // to the current state.
  size_t expected_tab_count =
      tab_stats_tracker_->AddTabs(12, this, tab_strip_modifier_.get());
  size_t expected_window_count = tab_stats_tracker_->AddWindows(5);
  size_t expected_max_tab_per_window = expected_tab_count;
  tab_stats_tracker_->data_store()->UpdateMaxTabsPerWindowIfNeeded(
      expected_max_tab_per_window);
  expected_tab_count =
      tab_stats_tracker_->RemoveTabs(5, tab_strip_modifier_.get());
  expected_window_count = tab_stats_tracker_->RemoveWindows(2);
  expected_max_tab_per_window = expected_tab_count;

  TabsStats stats = tab_stats_tracker_->data_store()->tab_stats();

  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kUnknown);
  // Trigger the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // Ensures that the histograms have been properly updated.
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kMaxTabsInADayHistogramName,
      stats.total_tab_count_max, 1);
  ExpectUniqueBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsInADayHistogramName),
      stats.total_tab_count_max, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName,
      stats.max_tab_per_window, 1);
  ExpectUniqueBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName),
      stats.max_tab_per_window, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName,
      stats.window_count_max, 1);
  ExpectUniqueBucketedSample(
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

  power_monitor_source_.GeneratePowerStateEvent(
      base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);
  EXPECT_EQ(power_monitor_source_.GetBatteryPowerStatus(),
            base::PowerStateObserver::BatteryPowerStatus::kBatteryPower);

  // Trigger the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // The values in the histograms should now be equal to the current state.
  ExpectBucketedSample(UmaStatsReportingDelegate::kMaxTabsInADayHistogramName,
                       stats.total_tab_count_max, 1);
  ExpectBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsInADayHistogramName),
      stats.total_tab_count_max, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName,
      stats.max_tab_per_window, 1);
  ExpectBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxTabsPerWindowInADayHistogramName),
      stats.max_tab_per_window, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName,
      stats.window_count_max, 1);
  ExpectBucketedSample(
      GetHistogramNameWithBatteryStateSuffix(
          UmaStatsReportingDelegate::kMaxWindowsInADayHistogramName),
      stats.window_count_max, 1);
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/412634171): Enable this when discarding is supported on
// Android.
TEST_F(TabStatsTrackerTest, DailyDiscards) {
  // This test checks that the discard/reload counts are reported when the
  // daily event triggers.

  // Daily report is skipped when there is no tab. Adds tabs to avoid that.
  tab_stats_tracker_->AddTabs(1, this, tab_strip_modifier_.get());

  constexpr size_t kExpectedDiscardsExternal = 1;
  constexpr size_t kExpectedDiscardsUrgent = 2;
  constexpr size_t kExpectedDiscardsProactive = 3;
  constexpr size_t kExpectedDiscardsSuggested = 4;
  constexpr size_t kExpectedDiscardsFrozenWithGrowingMemory = 5;
  constexpr size_t kExpectedReloadsExternal = 6;
  constexpr size_t kExpectedReloadsUrgent = 7;
  constexpr size_t kExpectedReloadsProactive = 8;
  constexpr size_t kExpectedReloadsSuggested = 9;
  constexpr size_t kExpectedReloadsFrozenWithGrowingMemory = 10;
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
  for (size_t i = 0; i < kExpectedDiscardsFrozenWithGrowingMemory; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::FROZEN_WITH_GROWING_MEMORY,
        /*is_discarded*/ true);
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
  for (size_t i = 0; i < kExpectedReloadsFrozenWithGrowingMemory; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::FROZEN_WITH_GROWING_MEMORY,
        /*is_discarded*/ false);
  }

  // Triggers the daily event.
  tab_stats_tracker_->TriggerDailyEvent();

  // Checks that the histograms have been properly updated.
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsExternalHistogramName,
      kExpectedDiscardsExternal, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsUrgentHistogramName,
      kExpectedDiscardsUrgent, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsProactiveHistogramName,
      kExpectedDiscardsProactive, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsSuggestedHistogramName,
      kExpectedDiscardsSuggested, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::
          kDailyDiscardsFrozenWithGrowingMemoryHistogramName,
      kExpectedDiscardsFrozenWithGrowingMemory, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsExternalHistogramName,
      kExpectedReloadsExternal, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsUrgentHistogramName,
      kExpectedReloadsUrgent, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsProactiveHistogramName,
      kExpectedReloadsProactive, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsSuggestedHistogramName,
      kExpectedReloadsSuggested, 1);
  ExpectUniqueBucketedSample(
      UmaStatsReportingDelegate::
          kDailyReloadsFrozenWithGrowingMemoryHistogramName,
      kExpectedReloadsFrozenWithGrowingMemory, 1);

  // Checks that the second report also updates the histograms properly.
  constexpr size_t kExpectedDiscardsExternal2 = 11;
  constexpr size_t kExpectedDiscardsUrgent2 = 12;
  constexpr size_t kExpectedDiscardsProactive2 = 13;
  constexpr size_t kExpectedDiscardsSuggested2 = 14;
  constexpr size_t kExpectedDiscardsFrozenWithGrowingMemory2 = 15;
  constexpr size_t kExpectedReloadsExternal2 = 16;
  constexpr size_t kExpectedReloadsUrgent2 = 17;
  constexpr size_t kExpectedReloadsProactive2 = 18;
  constexpr size_t kExpectedReloadsSuggested2 = 19;
  constexpr size_t kExpectedReloadsFrozenWithGrowingMemory2 = 20;
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
  for (size_t i = 0; i < kExpectedDiscardsFrozenWithGrowingMemory2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::FROZEN_WITH_GROWING_MEMORY,
        /*is_discarded=*/true);
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
  for (size_t i = 0; i < kExpectedReloadsFrozenWithGrowingMemory2; ++i) {
    tab_stats_tracker_->DiscardedStateChange(
        this, LifecycleUnitDiscardReason::FROZEN_WITH_GROWING_MEMORY,
        /*is_discarded=*/false);
  }

  // Triggers the daily event again.
  tab_stats_tracker_->TriggerDailyEvent();

  // Checks that the histograms have been properly updated.
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsExternalHistogramName,
      kExpectedDiscardsExternal2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsUrgentHistogramName,
      kExpectedDiscardsUrgent2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsProactiveHistogramName,
      kExpectedDiscardsProactive2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyDiscardsSuggestedHistogramName,
      kExpectedDiscardsSuggested2, 1);
  ExpectBucketedSample(UmaStatsReportingDelegate::
                           kDailyDiscardsFrozenWithGrowingMemoryHistogramName,
                       kExpectedDiscardsFrozenWithGrowingMemory2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsExternalHistogramName,
      kExpectedReloadsExternal2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsUrgentHistogramName,
      kExpectedReloadsUrgent2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsProactiveHistogramName,
      kExpectedReloadsProactive2, 1);
  ExpectBucketedSample(
      UmaStatsReportingDelegate::kDailyReloadsSuggestedHistogramName,
      kExpectedReloadsSuggested2, 1);
  ExpectBucketedSample(UmaStatsReportingDelegate::
                           kDailyReloadsFrozenWithGrowingMemoryHistogramName,
                       kExpectedReloadsFrozenWithGrowingMemory2, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(TabStatsTrackerTest, HeartbeatMetrics) {
  size_t expected_tab_count =
      tab_stats_tracker_->AddTabs(12, this, tab_strip_modifier_.get());
  size_t expected_window_count = tab_stats_tracker_->AddWindows(5);

  tab_stats_tracker_->OnHeartbeatEvent();

  ExpectBucketedSample(UmaStatsReportingDelegate::kTabCountHistogramName,
                       expected_tab_count, 1);
  ExpectBucketedSample(UmaStatsReportingDelegate::kWindowCountHistogramName,
                       expected_window_count, 1);

  expected_tab_count =
      tab_stats_tracker_->RemoveTabs(4, tab_strip_modifier_.get());
  expected_window_count = tab_stats_tracker_->RemoveWindows(3);

  tab_stats_tracker_->OnHeartbeatEvent();

  ExpectBucketedSample(UmaStatsReportingDelegate::kWindowCountHistogramName,
                       expected_window_count, 1);
}

TEST_F(TabStatsTrackerTest, VideoPlayingInTab) {
  content::WebContentsTester* const contents_tester =
      content::WebContentsTester::For(web_contents());

  constexpr auto kStoppedReason =
      content::WebContentsObserver::MediaStoppedReason::kReachedEndOfStream;

  const content::MediaPlayerId video_player_id0(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);
  const content::WebContentsObserver::MediaPlayerInfo video_player_info0(
      /*has_video=*/true, /*has_audio=*/true);

  const content::MediaPlayerId video_player_id1(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 1);
  const content::WebContentsObserver::MediaPlayerInfo video_player_info1(
      /*has_video=*/true, /*has_audio=*/false);

  const content::MediaPlayerId audio_video_player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 2);
  content::WebContentsObserver::MediaPlayerInfo audio_video_player_info(
      /*has_video=*/false, /*has_audio=*/true);

  TestTabStatsObserver tab_stats_observer(*tab_stats_tracker_);

  tab_stats_tracker_->OnInitialOrInsertedTab(web_contents());

  content::WebContentsObserver* const observer =
      tab_stats_tracker_->GetWebContentsUsageObserverForTesting(web_contents());
  ASSERT_NE(observer, nullptr);

  EXPECT_FALSE(tab_stats_observer.is_video_playing_in_tab());

  observer->MediaMetadataChanged(video_player_info0, video_player_id0);
  observer->MediaMetadataChanged(video_player_info1, video_player_id1);
  observer->MediaMetadataChanged(audio_video_player_info,
                                 audio_video_player_id);
  EXPECT_FALSE(tab_stats_observer.is_video_playing_in_tab());

  observer->MediaStartedPlaying(audio_video_player_info, audio_video_player_id);
  EXPECT_FALSE(tab_stats_observer.is_video_playing_in_tab())
      << "Only audio is playing";

  contents_tester->SetCurrentlyPlayingVideoCount(1);
  observer->MediaStartedPlaying(video_player_info0, video_player_id0);
  EXPECT_TRUE(tab_stats_observer.is_video_playing_in_tab())
      << "One video is playing";

  contents_tester->SetCurrentlyPlayingVideoCount(2);
  observer->MediaStartedPlaying(video_player_info1, video_player_id1);
  EXPECT_TRUE(tab_stats_observer.is_video_playing_in_tab())
      << "Two videos are playing";

  contents_tester->SetCurrentlyPlayingVideoCount(1);
  observer->MediaStoppedPlaying(video_player_info1, video_player_id1,
                                kStoppedReason);
  EXPECT_TRUE(tab_stats_observer.is_video_playing_in_tab())
      << "One video is playing";

  contents_tester->SetCurrentlyPlayingVideoCount(0);
  observer->MediaStoppedPlaying(video_player_info0, video_player_id0,
                                kStoppedReason);
  EXPECT_FALSE(tab_stats_observer.is_video_playing_in_tab())
      << "No video is playing";

  audio_video_player_info.has_video = true;
  contents_tester->SetCurrentlyPlayingVideoCount(1);
  observer->MediaMetadataChanged(audio_video_player_info,
                                 audio_video_player_id);
  EXPECT_TRUE(tab_stats_observer.is_video_playing_in_tab())
      << "A video track was added to a player that was playing";

  audio_video_player_info.has_video = false;
  contents_tester->SetCurrentlyPlayingVideoCount(0);
  observer->MediaMetadataChanged(audio_video_player_info,
                                 audio_video_player_id);
  EXPECT_FALSE(tab_stats_observer.is_video_playing_in_tab())
      << "The video track was removed";

  observer->MediaStoppedPlaying(audio_video_player_info, audio_video_player_id,
                                kStoppedReason);
  EXPECT_FALSE(tab_stats_observer.is_video_playing_in_tab())
      << "No video is playing";
}

}  // namespace metrics
