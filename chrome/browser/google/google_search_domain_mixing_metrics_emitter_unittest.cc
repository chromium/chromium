// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_search_domain_mixing_metrics_emitter.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "components/history/core/browser/domain_mixing_metrics.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class GoogleSearchDomainMixingMetricsEmitterTest : public testing::Test {
 public:
  GoogleSearchDomainMixingMetricsEmitterTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    GoogleSearchDomainMixingMetricsEmitter::RegisterProfilePrefs(
        prefs_.registry());

    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(history_dir_.GetPath(),
                                                     /*create_db=*/true);

    emitter_ = std::make_unique<GoogleSearchDomainMixingMetricsEmitter>(
        &prefs_, history_service_.get());

    auto clock = std::make_unique<base::SimpleTestClock>();
    clock_ = clock.get();
    emitter_->SetClockForTesting(std::move(clock));

    auto timer = std::make_unique<base::MockRepeatingTimer>();
    timer_ = timer.get();
    emitter_->SetTimerForTesting(std::move(timer));

    emitter_->SetUIThreadTaskRunnerForTesting(
        task_environment_.GetMainThreadTaskRunner());
  }

  // Sets up test history such that domain mixing metrics for the day starting
  // at last_metrics_time are what is verified by VerifyHistograms().
  void SetUpTestHistory(base::Time last_metrics_time) {
    // Out of range for the 30 day domain mixing metric.
    history_service_->AddPage(GURL("https://www.google.de/search?q=foo"),
                              last_metrics_time - base::TimeDelta::FromDays(30),
                              history::SOURCE_BROWSED);
    // First event in range for the 30 day domain mixing metric.
    history_service_->AddPage(GURL("https://www.google.fr/search?q=foo"),
                              last_metrics_time - base::TimeDelta::FromDays(29),
                              history::SOURCE_BROWSED);
    history_service_->AddPage(GURL("https://www.google.com/search?q=foo"),
                              last_metrics_time, history::SOURCE_BROWSED);
    history_service_->AddPage(
        GURL("https://www.google.com/search?q=foo"),
        last_metrics_time + base::TimeDelta::FromHours(23),
        history::SOURCE_BROWSED);
    // Out of range for the day of metrics to compute.
    history_service_->AddPage(
        GURL("https://www.google.ch/search?q=foo"),
        last_metrics_time + base::TimeDelta::FromHours(24),
        history::SOURCE_BROWSED);
  }

  void VerifyHistograms(const base::HistogramTester& tester) {
    tester.ExpectUniqueSample("DomainMixing.OneDay", 0, 1);
    tester.ExpectUniqueSample("DomainMixing.OneWeek", 0, 1);
    tester.ExpectUniqueSample("DomainMixing.TwoWeeks", 0, 1);
    tester.ExpectUniqueSample("DomainMixing.OneMonth", 33, 1);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<GoogleSearchDomainMixingMetricsEmitter> emitter_;
  base::SimpleTestClock* clock_;  // Not owned.
  base::MockRepeatingTimer* timer_;  // Not owned.
};

TEST_F(GoogleSearchDomainMixingMetricsEmitterTest, FirstStart) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromString("01 Jan 2018 12:00:00", &now));
  clock_->SetNow(now);
  // The last metrics time should be initialized to 4am on the same day, so that
  // metrics are from now on computed for daily windows anchored at 4am.
  base::Time expected_last_metrics_time;
  ASSERT_TRUE(base::Time::FromString("01 Jan 2018 04:00:00",
                                     &expected_last_metrics_time));

  emitter_->Start();

  EXPECT_EQ(
      expected_last_metrics_time,
      prefs_.GetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime));
  // The next metric calculation should be scheduled at 4am on the next day,
  // i.e. 16 hours from |now|.
  EXPECT_EQ(base::TimeDelta::FromHours(16),
            task_environment_.NextMainThreadPendingTaskDelay());
}

TEST_F(GoogleSearchDomainMixingMetricsEmitterTest, Waits10SecondsAfterStart) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromString("01 Jan 2018 12:00:00", &now));
  clock_->SetNow(now);
  // Metrics were last computed a day ago and need to be recomputed immediately.
  prefs_.SetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime,
                 now - base::TimeDelta::FromDays(1));

  emitter_->Start();

  EXPECT_EQ(base::TimeDelta::FromSeconds(10),
            task_environment_.NextMainThreadPendingTaskDelay());
}

TEST_F(GoogleSearchDomainMixingMetricsEmitterTest, WaitsUntilNeeded) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromString("01 Jan 2018 12:00:00", &now));
  clock_->SetNow(now);
  // Metrics were last computed an hour ago and the emitter should wait 23 hours
  // before emitting new metrics.
  prefs_.SetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime,
                 now - base::TimeDelta::FromHours(1));

  emitter_->Start();

  EXPECT_EQ(base::TimeDelta::FromHours(23),
            task_environment_.NextMainThreadPendingTaskDelay());
}

// Disabled pending deletion, see https://crbug.com/1040458
// This test takes several seconds of CPU time to run because it simulates a
// month worth of history expiration running, which happens every 300 seconds.
TEST_F(GoogleSearchDomainMixingMetricsEmitterTest,
       DISABLED_EmitsMetricsOnStart) {
  // Metrics were computed up to 4am on Jan 1st.
  base::Time last_metrics_time;
  ASSERT_TRUE(
      base::Time::FromString("01 Jan 2018 04:00:00", &last_metrics_time));
  prefs_.SetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime,
                 last_metrics_time);
  // It is now 12pm on Jan 2nd, so metrics for Jan 1st 4am to Jan 2nd 4am should
  // be computed.
  base::Time now;
  ASSERT_TRUE(base::Time::FromString("02 Jan 2018 12:00:00", &now));
  clock_->SetNow(now);
  SetUpTestHistory(last_metrics_time);

  emitter_->Start();

  base::HistogramTester tester;

  // Fast forward far enough that histograms have been written to for all
  // intervals.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromDays(history::kOneMonth));
  BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  VerifyHistograms(tester);
}

TEST_F(GoogleSearchDomainMixingMetricsEmitterTest,
       DISABLED_EmitsMetricsWhenTimerFires) {
  // Metrics were computed up to 4am on Jan 1st.
  base::Time last_metrics_time;
  ASSERT_TRUE(
      base::Time::FromString("01 Jan 2018 04:00:00", &last_metrics_time));
  prefs_.SetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime,
                 last_metrics_time);
  // It is now 12pm on Jan 2nd, so metrics for Jan 1st 4am to Jan 2nd 4am should
  // be computed.
  base::Time now;
  ASSERT_TRUE(base::Time::FromString("02 Jan 2018 12:00:00", &now));
  clock_->SetNow(now);

  // Start the emitter.
  emitter_->Start();

  // Fast forward far enough that histograms have been written to for all
  // intervals.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromDays(history::kOneMonth));
  BlockUntilHistoryProcessesPendingRequests(history_service_.get());

  // last_metrics_time is expected to have been incremented.
  last_metrics_time += base::TimeDelta::FromDays(1);
  EXPECT_EQ(
      last_metrics_time,
      prefs_.GetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime));

  // The timer is expected to trigger a day later.
  EXPECT_EQ(base::TimeDelta::FromDays(1), timer_->GetCurrentDelay());

  // A day later, metrics should be recomputed without a call to Start() when
  // the timer triggers.
  ASSERT_TRUE(base::Time::FromString("03 Jan 2018 12:00:00", &now));
  clock_->SetNow(now);
  SetUpTestHistory(last_metrics_time);

  timer_->Fire();

  base::HistogramTester tester;
  // Fast forward far enough that histograms have been written to for all
  // intervals.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromDays(history::kOneMonth));
  BlockUntilHistoryProcessesPendingRequests(history_service_.get());
  VerifyHistograms(tester);

  // last_metrics_time is expected to have been incremented.
  last_metrics_time += base::TimeDelta::FromDays(1);
  EXPECT_EQ(
      last_metrics_time,
      prefs_.GetTime(GoogleSearchDomainMixingMetricsEmitter::kLastMetricsTime));
}
