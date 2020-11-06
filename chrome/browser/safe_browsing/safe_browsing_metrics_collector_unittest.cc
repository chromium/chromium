// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector.h"
#include <memory>

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/test_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingMetricsCollectorTest : public ::testing::Test {
 public:
  SafeBrowsingMetricsCollectorTest() = default;

  void SetUp() override {
    task_environment_ = CreateTestTaskEnvironment(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    RegisterPrefs();
    metrics_collector_ =
        std::make_unique<SafeBrowsingMetricsCollector>(&pref_service_);
  }

 protected:
  void SetSafeBrowsingMetricsLastLogTime(base::Time time) {
    pref_service_.SetInt64(prefs::kSafeBrowsingMetricsLastLogTime,
                           time.ToDeltaSinceWindowsEpoch().InSeconds());
  }

  std::unique_ptr<SafeBrowsingMetricsCollector> metrics_collector_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  TestingPrefServiceSimple pref_service_;

 private:
  void RegisterPrefs() {
    pref_service_.registry()->RegisterInt64Pref(
        prefs::kSafeBrowsingMetricsLastLogTime, 0);
    pref_service_.registry()->RegisterBooleanPref(prefs::kSafeBrowsingEnabled,
                                                  true),
        pref_service_.registry()->RegisterBooleanPref(
            prefs::kSafeBrowsingEnhanced, false);
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutReportingEnabled, false);
  }
};

TEST_F(SafeBrowsingMetricsCollectorTest,
       TestLastLoggingIntervalLongerThanScheduleInterval) {
  base::HistogramTester histograms;
  SetSafeBrowsingMetricsLastLogTime(base::Time::Now() -
                                    base::TimeDelta::FromHours(25));
  SetSafeBrowsingState(&pref_service_, STANDARD_PROTECTION);
  SetExtendedReportingPrefForTests(&pref_service_, true);
  metrics_collector_->StartLogging();
  // Should log immediately.
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 1);
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.Extended",
                               /* sample */ 1, /* expected_count */ 1);
  task_environment_->FastForwardBy(base::TimeDelta::FromHours(23));
  // Shouldn't log new data before the scheduled time.
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 1);
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.Extended",
                               /* sample */ 1, /* expected_count */ 1);
  task_environment_->FastForwardBy(base::TimeDelta::FromHours(1));
  // Should log when the scheduled time arrives.
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 2);
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.Extended",
                               /* sample */ 1, /* expected_count */ 2);
  task_environment_->FastForwardBy(base::TimeDelta::FromHours(24));
  // Should log when the scheduled time arrives.
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 3);
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.Extended",
                               /* sample */ 1, /* expected_count */ 3);
}

TEST_F(SafeBrowsingMetricsCollectorTest,
       TestLastLoggingIntervalShorterThanScheduleInterval) {
  base::HistogramTester histograms;
  SetSafeBrowsingMetricsLastLogTime(base::Time::Now() -
                                    base::TimeDelta::FromHours(1));
  SetSafeBrowsingState(&pref_service_, STANDARD_PROTECTION);
  metrics_collector_->StartLogging();
  // Should not log immediately because the last logging interval is shorter
  // than the interval.
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 0);
  task_environment_->FastForwardBy(base::TimeDelta::FromHours(23));
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 1);
  task_environment_->FastForwardBy(base::TimeDelta::FromHours(24));
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 2);
}

TEST_F(SafeBrowsingMetricsCollectorTest, TestPrefChangeBetweenLogging) {
  base::HistogramTester histograms;
  SetSafeBrowsingMetricsLastLogTime(base::Time::Now() -
                                    base::TimeDelta::FromHours(25));
  SetSafeBrowsingState(&pref_service_, STANDARD_PROTECTION);
  metrics_collector_->StartLogging();
  histograms.ExpectTotalCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                              /* expected_count */ 1);
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 1, /* expected_count */ 1);
  SetSafeBrowsingState(&pref_service_, NO_SAFE_BROWSING);
  task_environment_->FastForwardBy(base::TimeDelta::FromHours(24));
  histograms.ExpectTotalCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                              /* expected_count */ 2);
  histograms.ExpectBucketCount("SafeBrowsing.Pref.Daily.SafeBrowsingState",
                               /* sample */ 0, /* expected_count */ 1);
}
}  // namespace safe_browsing
