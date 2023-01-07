// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

using DailyUsageType = OfflineMetricsCollectorImpl::DailyUsageType;

class OfflineMetricsCollectorTest : public testing::Test {
 public:
  OfflineMetricsCollectorTest() {}

  OfflineMetricsCollectorTest(const OfflineMetricsCollectorTest&) = delete;
  OfflineMetricsCollectorTest& operator=(const OfflineMetricsCollectorTest&) =
      delete;

  ~OfflineMetricsCollectorTest() override {}

  // testing::Test:
  void SetUp() override {
    base::Time epoch;
    ASSERT_TRUE(base::Time::FromUTCString("1 Jan 1994 GMT", &epoch));
    test_clock()->SetNow(epoch.LocalMidnight());
    OfflineMetricsCollectorImpl::RegisterPrefs(pref_service_.registry());
    Reload();
  }

  // This creates new collector which will read the initial values from Prefs.
  void Reload() {
    collector_ =
        std::make_unique<OfflineMetricsCollectorImpl>(&prefs());
  }

  TestScopedOfflineClock* test_clock() { return &test_clock_; }
  PrefService& prefs() { return pref_service_; }
  OfflineMetricsCollector* collector() const { return collector_.get(); }
  const base::HistogramTester& histograms() const { return histogram_tester_; }

  base::Time GetTimestampFromPrefs() {
    return prefs().GetTime(prefs::kOfflineUsageTrackingDay);
  }

  void ExpectOfflineUsageBucketCount(DailyUsageType bucket, int count) {
    histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                   static_cast<int>(bucket), count);
  }

  void ExpectNotResilientOfflineUsageBucketCount(DailyUsageType bucket,
                                                 int count) {
    histograms().ExpectBucketCount(
        "OfflinePages.OfflineUsage.NotOfflineResilient",
        static_cast<int>(bucket), count);
  }

  void ExpectOfflineUsageTotalCount(int count) {
    histograms().ExpectTotalCount("OfflinePages.OfflineUsage", count);
  }

  void ExpectNotResilientOfflineUsageTotalCount(int count) {
    histograms().ExpectTotalCount(
        "OfflinePages.OfflineUsage.NotOfflineResilient", count);
  }

 private:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<OfflineMetricsCollectorImpl> collector_;
  base::HistogramTester histogram_tester_;
  TestScopedOfflineClock test_clock_;
};

TEST_F(OfflineMetricsCollectorTest, CheckCleanInit) {
  EXPECT_EQ(0L, prefs().GetInt64(prefs::kOfflineUsageTrackingDay));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageEnabledObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageFetchObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageOpenObserved));

  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageUnusedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageStartedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOfflineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOnlineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageMixedCount));

  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageEnabledCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageFetchedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageOpenedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageMixedCount));

  // No offline usage metrics should have been be reported.
  ExpectOfflineUsageTotalCount(0);
  ExpectNotResilientOfflineUsageTotalCount(0);
}

TEST_F(OfflineMetricsCollectorTest, FirstStart) {
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  base::Time start = test_clock()->Now();

  collector()->OnAppStartupOrResume();

  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  // Timestamp shouldn't change.
  EXPECT_EQ(GetTimestampFromPrefs(), start);
  // Accumulated counters shouldn't change.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageUnusedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageStartedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOfflineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOnlineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageMixedCount));

  // No offline usage metrics should have been be reported.
  ExpectOfflineUsageTotalCount(0);
  ExpectNotResilientOfflineUsageTotalCount(0);
}

TEST_F(OfflineMetricsCollectorTest, SetTrackingFlags) {
  collector()->OnAppStartupOrResume();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  collector()->OnSuccessfulNavigationOffline();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  collector()->OnSuccessfulNavigationOnline();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  // No offline usage metrics should have been be reported.
  ExpectOfflineUsageTotalCount(0);
  ExpectNotResilientOfflineUsageTotalCount(0);
}

TEST_F(OfflineMetricsCollectorTest, SetTrackingFlagsPrefech) {
  collector()->OnPrefetchEnabled();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageEnabledObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageFetchObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageOpenObserved));
  collector()->OnSuccessfulPagePrefetch();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageEnabledObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageFetchObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageOpenObserved));
  collector()->OnPrefetchedPageOpened();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageEnabledObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageFetchObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageOpenObserved));
}

TEST_F(OfflineMetricsCollectorTest, TrueIsFinalState) {
  collector()->OnAppStartupOrResume();
  collector()->OnSuccessfulNavigationOnline();
  collector()->OnSuccessfulNavigationOffline();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  collector()->OnSuccessfulNavigationOffline();
  collector()->OnSuccessfulNavigationOnline();
  collector()->OnAppStartupOrResume();
  // once 'true', tracking flags do not change.
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  // No offline usage metrics should have been be reported.
  ExpectOfflineUsageTotalCount(0);
  ExpectNotResilientOfflineUsageTotalCount(0);
}

// Restore from Prefs keeps accumulated state, counters and timestamp.
TEST_F(OfflineMetricsCollectorTest, RestoreFromPrefs) {
  base::Time start = test_clock()->Now();
  collector()->OnSuccessfulNavigationOnline();
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  EXPECT_EQ(GetTimestampFromPrefs(), start);

  prefs().SetInteger(prefs::kOfflineUsageUnusedCount, 1);
  prefs().SetInteger(prefs::kOfflineUsageStartedCount, 2);
  prefs().SetInteger(prefs::kOfflineUsageOfflineCount, 3);
  prefs().SetInteger(prefs::kOfflineUsageOnlineCount, 4);
  prefs().SetInteger(prefs::kOfflineUsageMixedCount, 5);

  Reload();
  collector()->OnSuccessfulNavigationOffline();
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  EXPECT_EQ(GetTimestampFromPrefs(), start);

  // No offline resilient metrics should have been be reported up to this point.
  ExpectOfflineUsageTotalCount(0);

  collector()->ReportAccumulatedStats();
  ExpectOfflineUsageBucketCount(DailyUsageType::kUnused, 1);
  ExpectOfflineUsageBucketCount(DailyUsageType::kStarted, 2);
  ExpectOfflineUsageBucketCount(DailyUsageType::kOffline, 3);
  ExpectOfflineUsageBucketCount(DailyUsageType::kOnline, 4);
  ExpectOfflineUsageBucketCount(DailyUsageType::kMixed, 5);

  // As the reported metrics are all from restored values, there should be no
  // values reported to the non-resilient metric.
  ExpectNotResilientOfflineUsageTotalCount(0);

  // After reporting, counters should be reset.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageUnusedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageStartedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOfflineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOnlineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageMixedCount));
}

TEST_F(OfflineMetricsCollectorTest, RestoreFromPrefsPrefetch) {
  collector()->OnPrefetchEnabled();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageEnabledObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageFetchObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageOpenObserved));

  prefs().SetInteger(prefs::kPrefetchUsageEnabledCount, 1);
  prefs().SetInteger(prefs::kPrefetchUsageFetchedCount, 2);
  prefs().SetInteger(prefs::kPrefetchUsageOpenedCount, 3);
  prefs().SetInteger(prefs::kPrefetchUsageMixedCount, 4);

  Reload();
  collector()->OnSuccessfulPagePrefetch();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageEnabledObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kPrefetchUsageFetchObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kPrefetchUsageOpenObserved));

  collector()->ReportAccumulatedStats();
  histograms().ExpectBucketCount("OfflinePages.PrefetchEnabled", true, 1);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 1 /* PrefetchUsageType::kFetchedNewPages */,
                                 2);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 2 /* PrefetchUsageType::kOpenedPages */, 3);
  histograms().ExpectBucketCount(
      "OfflinePages.PrefetchUsage",
      3 /* PrefetchUsageType::kFetchedAndOpenedPages */, 4);

  // After reporting, counters should be reset.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageEnabledCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageFetchedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageOpenedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageMixedCount));
}

TEST_F(OfflineMetricsCollectorTest, ChangesWithinDay) {
  base::Time start = test_clock()->Now();
  collector()->OnAppStartupOrResume();
  collector()->OnSuccessfulNavigationOnline();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  // Move time ahead but still same day.
  test_clock()->Advance(base::Hours(1));
  collector()->OnSuccessfulNavigationOffline();
  // Timestamp shouldn't change.
  EXPECT_EQ(GetTimestampFromPrefs(), start);

  // Counters should not be affected.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageUnusedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageStartedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOfflineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOnlineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageMixedCount));

  // No offline usage metrics should have been be reported.
  ExpectOfflineUsageTotalCount(0);
  ExpectNotResilientOfflineUsageTotalCount(0);
}

TEST_F(OfflineMetricsCollectorTest, MultipleDays) {
  // Clock starts at epoch.LocalMidnight()
  collector()->OnAppStartupOrResume();

  ExpectNotResilientOfflineUsageTotalCount(0);

  // Advance the clock to the next day
  test_clock()->Advance(base::Hours(25));

  collector()->OnAppStartupOrResume();
  // 1 day 'started' counter, another is being tracked as current day...
  EXPECT_EQ(1, prefs().GetInteger(prefs::kOfflineUsageStartedCount));

  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  // Non-resilient metrics are reported for past days.
  ExpectNotResilientOfflineUsageBucketCount(DailyUsageType::kStarted, 1);
  ExpectNotResilientOfflineUsageTotalCount(1);

  // Skip the next 4 days within the virtual clock
  test_clock()->Advance(base::Days(4));
  collector()->OnSuccessfulNavigationOnline();
  // 2 days started, 3 days skipped ('unused').
  EXPECT_EQ(2, prefs().GetInteger(prefs::kOfflineUsageStartedCount));
  EXPECT_EQ(3, prefs().GetInteger(prefs::kOfflineUsageUnusedCount));

  // No online days in the past..
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOnlineCount));
  // .. but current day is 'online' type.
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  // Other counters not affected.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOfflineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageMixedCount));

  // Non-resilient metrics are reported for past days.
  ExpectNotResilientOfflineUsageBucketCount(DailyUsageType::kStarted, 2);
  ExpectNotResilientOfflineUsageBucketCount(DailyUsageType::kUnused, 3);
  ExpectNotResilientOfflineUsageTotalCount(5);

  // Up to this point, no offline resilient metrics should be reported.
  ExpectOfflineUsageTotalCount(0);

  // Force collector to report stats and observe them reported correctly.
  collector()->ReportAccumulatedStats();
  ExpectOfflineUsageBucketCount(DailyUsageType::kUnused, 3);
  ExpectOfflineUsageBucketCount(DailyUsageType::kStarted, 2);
  ExpectNotResilientOfflineUsageTotalCount(5);
  ExpectOfflineUsageTotalCount(5);
}

TEST_F(OfflineMetricsCollectorTest, OverDayBoundaryPrefetch) {
  // Clock starts at epoch.LocalMidnight()
  collector()->OnPrefetchEnabled();

  test_clock()->Advance(base::Days(1));
  collector()->OnPrefetchEnabled();

  test_clock()->Advance(base::Days(1));
  collector()->OnSuccessfulPagePrefetch();

  test_clock()->Advance(base::Days(1));
  collector()->OnPrefetchedPageOpened();

  test_clock()->Advance(base::Days(1));
  collector()->OnPrefetchEnabled();
  collector()->OnSuccessfulPagePrefetch();
  collector()->OnPrefetchedPageOpened();

  test_clock()->Advance(base::Days(1));
  collector()->OnPrefetchEnabled();

  // Force collector to report stats and observe them reported correctly.
  collector()->ReportAccumulatedStats();
  histograms().ExpectBucketCount("OfflinePages.PrefetchEnabled", true, 3);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 1 /* PrefetchUsageType::kFetchedNewPages */,
                                 1);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 2 /* PrefetchUsageType::kOpenedPages */, 1);
  histograms().ExpectBucketCount(
      "OfflinePages.PrefetchUsage",
      3 /* PrefetchUsageType::kFetchedAndOpenedPages */, 1);
}

}  // namespace offline_pages
