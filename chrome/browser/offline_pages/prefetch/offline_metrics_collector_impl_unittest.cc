// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
using UsageType = OfflineMetricsCollectorImpl::DailyUsageType;

class OfflineMetricsCollectorTest : public testing::Test {
 public:
  OfflineMetricsCollectorTest() {}
  ~OfflineMetricsCollectorTest() override {}

  // testing::Test:
  void SetUp() override {
    test_clock().SetNow(base::Time::Now().LocalMidnight());
    OfflineMetricsCollectorImpl::RegisterPrefs(pref_service_.registry());
    Reload();
  }

  // This creates new collector whcih will read the initial values from Prefs.
  void Reload() {
    collector_ =
        std::make_unique<OfflineMetricsCollectorImpl>(&prefs());
    collector_->SetClockForTesting(&test_clock());
  }

  base::SimpleTestClock& test_clock() { return test_clock_; }
  PrefService& prefs() { return pref_service_; }
  OfflineMetricsCollector* collector() const { return collector_.get(); }
  const base::HistogramTester& histograms() const { return histogram_tester_; }

  base::Time GetTimestampFromPrefs() {
    return prefs().GetTime(prefs::kOfflineUsageTrackingDay);
  }

 protected:
  base::SimpleTestClock test_clock_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<OfflineMetricsCollectorImpl> collector_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(OfflineMetricsCollectorTest);
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
}

TEST_F(OfflineMetricsCollectorTest, FirstStart) {
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));
  base::Time start = test_clock().Now();

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
}

// Restore from Prefs keeps accumulated state, counters and timestamp.
TEST_F(OfflineMetricsCollectorTest, RestoreFromPrefs) {
  base::Time start = test_clock().Now();
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

  collector()->ReportAccumulatedStats();
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 0 /* UsageType::UNUSED */, 1);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 1 /* UsageType::STARTED */, 2);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 2 /* UsageType::OFFLINE */, 3);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 3 /* UsageType::ONLINE */, 4);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 4 /* UsageType::MIXED */, 5);

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
                                 1 /* PrefetchUsageType::FETCHED_NEW_PAGES */,
                                 2);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 2 /* PrefetchUsageType::OPENED_PAGES */, 3);
  histograms().ExpectBucketCount(
      "OfflinePages.PrefetchUsage",
      3 /* PrefetchUsageType::FETCHED_AND_OPENED_PAGES */, 4);

  // After reporting, counters should be reset.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageEnabledCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageFetchedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageOpenedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kPrefetchUsageMixedCount));
}

TEST_F(OfflineMetricsCollectorTest, ChangesWithinDay) {
  base::Time start = test_clock().Now();
  collector()->OnAppStartupOrResume();
  collector()->OnSuccessfulNavigationOnline();
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  // Move time ahead but still same day.
  base::Time later1Hour = start + base::TimeDelta::FromHours(1);
  test_clock().SetNow(later1Hour);
  collector()->OnSuccessfulNavigationOffline();
  // Timestamp shouldn't change.
  EXPECT_EQ(GetTimestampFromPrefs(), start);

  // Counters should not be affected.
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageUnusedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageStartedCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOfflineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageOnlineCount));
  EXPECT_EQ(0, prefs().GetInteger(prefs::kOfflineUsageMixedCount));
}

TEST_F(OfflineMetricsCollectorTest, MultipleDays) {
  base::Time start = test_clock().Now();
  collector()->OnAppStartupOrResume();

  base::Time nextDay = start + base::TimeDelta::FromHours(25);
  test_clock().SetNow(nextDay);

  collector()->OnAppStartupOrResume();
  // 1 day 'started' counter, another is being tracked as current day...
  EXPECT_EQ(1, prefs().GetInteger(prefs::kOfflineUsageStartedCount));

  EXPECT_EQ(true, prefs().GetBoolean(prefs::kOfflineUsageStartObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOfflineObserved));
  EXPECT_EQ(false, prefs().GetBoolean(prefs::kOfflineUsageOnlineObserved));

  base::Time skip4Days = nextDay + base::TimeDelta::FromHours(24 * 4);
  test_clock().SetNow(skip4Days);
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

  // Force collector to report stats and observe them reported correctly.
  collector()->ReportAccumulatedStats();
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 0 /* UsageType::UNUSED */, 3);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 1 /* UsageType::STARTED */, 2);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 2 /* UsageType::OFFLINE */, 0);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 3 /* UsageType::ONLINE */, 0);
  histograms().ExpectBucketCount("OfflinePages.OfflineUsage",
                                 4 /* UsageType::MIXED */, 0);
}

TEST_F(OfflineMetricsCollectorTest, OverDayBoundaryPrefetch) {
  base::Time start = test_clock().Now();
  collector()->OnPrefetchEnabled();

  test_clock().SetNow(start + base::TimeDelta::FromDays(1));
  collector()->OnPrefetchEnabled();

  test_clock().SetNow(start + base::TimeDelta::FromDays(2));
  collector()->OnSuccessfulPagePrefetch();

  test_clock().SetNow(start + base::TimeDelta::FromDays(3));
  collector()->OnPrefetchedPageOpened();

  test_clock().SetNow(start + base::TimeDelta::FromDays(4));
  collector()->OnPrefetchEnabled();
  collector()->OnSuccessfulPagePrefetch();
  collector()->OnPrefetchedPageOpened();

  test_clock().SetNow(start + base::TimeDelta::FromDays(6));
  collector()->OnPrefetchEnabled();

  // Force collector to report stats and observe them reported correctly.
  collector()->ReportAccumulatedStats();
  histograms().ExpectBucketCount("OfflinePages.PrefetchEnabled", true, 3);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 1 /* PrefetchUsageType::FETCHED_NEW_PAGES */,
                                 1);
  histograms().ExpectBucketCount("OfflinePages.PrefetchUsage",
                                 2 /* PrefetchUsageType::OPENED_PAGES */, 1);
  histograms().ExpectBucketCount(
      "OfflinePages.PrefetchUsage",
      3 /* PrefetchUsageType::FETCHED_AND_OPENED_PAGES */, 1);
}

}  // namespace offline_pages
