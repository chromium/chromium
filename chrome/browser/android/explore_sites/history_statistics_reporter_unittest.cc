// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/history/core/test/test_history_database.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Pref name for the persistent timestamp of the last stats reporting.
// Should be in sync with similar name in the reporter's impl.
const char kWeeklyStatsReportingTimestamp[] =
    "explore_sites.weekly_stats_reporting_timestamp";
}  // namespace

namespace explore_sites {

class HistoryStatisticsReporterTest : public testing::Test {
 public:
  HistoryStatisticsReporterTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}
  ~HistoryStatisticsReporterTest() override {}

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {history::HistoryService::kHistoryServiceUsesTaskScheduler}, {});

    HistoryStatisticsReporter::RegisterPrefs(pref_service_.registry());
    ASSERT_TRUE(history_dir_.CreateUniqueTempDir());
    // Creates HistoryService, but does not load it yet. Use LoadHistory() from
    // tests to control loading of HistoryService.
    history_service_ = std::make_unique<history::HistoryService>();
    reporter_ = std::make_unique<HistoryStatisticsReporter>(history_service(),
                                                            &pref_service_);
  }

  // Wait for separate background task runner in HistoryService to complete
  // all tasks and then all the tasks on the current one to complete as well.
  void RunUntilIdle() {
    history::BlockUntilHistoryProcessesPendingRequests(history_service());
    task_environment_.RunUntilIdle();
  }

  void ScheduleReportAndRunUntilIdle() {
    reporter()->ScheduleReportStatistics();
    task_environment_.FastForwardBy(
        HistoryStatisticsReporter::kComputeStatisticsDelay);
    RunUntilIdle();
  }

  bool LoadHistory() {
    if (!history_service_->Init(
            history::TestHistoryDatabaseParamsForPath(history_dir_.GetPath())))
      return false;
    history::BlockUntilHistoryProcessesPendingRequests(history_service());
    return true;
  }

  HistoryStatisticsReporter* reporter() const { return reporter_.get(); }
  const base::HistogramTester& histograms() const { return histogram_tester_; }
  history::HistoryService* history_service() { return history_service_.get(); }
  TestingPrefServiceSimple* prefs() { return &pref_service_; }

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir history_dir_;
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<HistoryStatisticsReporter> reporter_;

  DISALLOW_COPY_AND_ASSIGN(HistoryStatisticsReporterTest);
};

TEST_F(HistoryStatisticsReporterTest, HistoryNotLoaded) {
  EXPECT_FALSE(history_service()->backend_loaded());
  reporter()->ScheduleReportStatistics();

  // Move past initial delay of reporter.
  task_environment_.FastForwardBy(
      HistoryStatisticsReporter::kComputeStatisticsDelay);

  // Since History is not yet loaded, there should be no histograms.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 0);
  histograms().ExpectTotalCount("ExploreSites.MonthlyHostCount", 0);

  // Load history. This should trigger reporter, via HistoryService observer.
  ASSERT_TRUE(LoadHistory());
  RunUntilIdle();

  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 1);
  // No hosts were visited, but there should be a sample.
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 0, 1);
}

TEST_F(HistoryStatisticsReporterTest, HistoryLoaded) {
  EXPECT_FALSE(history_service()->backend_loaded());
  ASSERT_TRUE(LoadHistory());

  reporter()->ScheduleReportStatistics();
  // Move past initial delay of reporter.
  task_environment_.FastForwardBy(
      HistoryStatisticsReporter::kComputeStatisticsDelay);

  RunUntilIdle();
  // Since History is already loaded, there should be a sample reported.
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 0, 1);
}

TEST_F(HistoryStatisticsReporterTest, HistoryLoadedTimeDelay) {
  ASSERT_TRUE(LoadHistory());

  reporter()->ScheduleReportStatistics();
  RunUntilIdle();

  // No reporting yet because the initial delay of reporter prevents it
  // from accessing HistoryService for a while.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 0);

  // Move past initial delay of reporter.
  task_environment_.FastForwardBy(
      HistoryStatisticsReporter::kComputeStatisticsDelay);

  RunUntilIdle();
  // Since History is already loaded, there should be a sample reported.
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 0, 1);
}

TEST_F(HistoryStatisticsReporterTest, HostAddedSimple) {
  ASSERT_TRUE(LoadHistory());

  base::Time time_now = offline_pages::OfflineTimeNow();

  history_service()->AddPage(GURL("http://www.google.com"), time_now,
                             history::VisitSource::SOURCE_BROWSED);

  ScheduleReportAndRunUntilIdle();

  // One host.
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 1, 1);
}

TEST_F(HistoryStatisticsReporterTest, HostAddedLongAgo) {
  ASSERT_TRUE(LoadHistory());

  base::Time time_now = offline_pages::OfflineTimeNow();
  base::Time time_29_days_ago = time_now - base::TimeDelta::FromDays(29);
  base::Time time_31_days_ago = time_now - base::TimeDelta::FromDays(31);

  history_service()->AddPage(GURL("http://www.google.com"), time_now,
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://example.com"), time_29_days_ago,
                             history::VisitSource::SOURCE_BROWSED);
  history_service()->AddPage(GURL("http://example1.com"), time_31_days_ago,
                             history::VisitSource::SOURCE_BROWSED);

  ScheduleReportAndRunUntilIdle();

  // Two hosts, since the 3rd one was outside of the past month.
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 2, 1);
}

TEST_F(HistoryStatisticsReporterTest, OneRunPerSession) {
  ASSERT_TRUE(LoadHistory());

  base::Time time_now = offline_pages::OfflineTimeNow();

  history_service()->AddPage(GURL("http://www.google.com"), time_now,
                             history::VisitSource::SOURCE_BROWSED);

  ScheduleReportAndRunUntilIdle();

  // One query, one host.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 1);
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 1, 1);

  history_service()->AddPage(GURL("http://example.com"), time_now,
                             history::VisitSource::SOURCE_BROWSED);

  ScheduleReportAndRunUntilIdle();

  // Still one query, one host. Second query in the same session is ignored.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 1);
  histograms().ExpectUniqueSample("ExploreSites.MonthlyHostCount", 1, 1);
}

TEST_F(HistoryStatisticsReporterTest, OneRunPerWeekSaveTimestamp) {
  ASSERT_TRUE(LoadHistory());

  ScheduleReportAndRunUntilIdle();

  // One query.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 1);

  // Reporter should have left the time of request in Prefs.
  EXPECT_EQ(base::Time::Now(),
            prefs()->GetTime(kWeeklyStatsReportingTimestamp));
}

TEST_F(HistoryStatisticsReporterTest, OneRunPerWeekReadTimestamp) {
  ASSERT_TRUE(LoadHistory());

  prefs()->SetTime(kWeeklyStatsReportingTimestamp, base::Time::Now());
  ScheduleReportAndRunUntilIdle();

  // No queries, a week did not pass yet.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 0);
}

TEST_F(HistoryStatisticsReporterTest, OneRunPerWeekReadTimestampAfterWeek) {
  ASSERT_TRUE(LoadHistory());

  prefs()->SetTime(kWeeklyStatsReportingTimestamp,
                   base::Time::Now() - base::TimeDelta::FromDays(8));
  ScheduleReportAndRunUntilIdle();

  // More than a week since last query, should have gone through.
  histograms().ExpectTotalCount("History.DatabaseMonthlyHostCountTime", 1);
  // Reporter should have left the time of request in Prefs.
  EXPECT_EQ(base::Time::Now(),
            prefs()->GetTime(kWeeklyStatsReportingTimestamp));
}

}  // namespace explore_sites
