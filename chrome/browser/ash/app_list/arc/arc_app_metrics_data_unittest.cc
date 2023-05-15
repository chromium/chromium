// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_metrics_data.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";
constexpr char kHistogramBase[] = "Arc.Test.Histogram.";
constexpr char kTestTimeDeltaHistogram[] = "Arc.Test.Histogram.TimeDelta";
constexpr char kTestAppsIncompleteHistogram[] =
    "Arc.Test.Histogram.NumAppsIncomplete";
constexpr char kTestAppsRequestedHistogram[] =
    "Arc.Test.Histogram.NumAppsRequested";

}  // namespace

namespace arc {

class ArcAppMetricsDataTest : public testing::Test {
 public:
  ArcAppMetricsDataTest(const ArcAppMetricsDataTest&) = delete;
  ArcAppMetricsDataTest& operator=(const ArcAppMetricsDataTest&) = delete;

  void SetUp() override {
    arc_app_metrics_data_ = std::make_unique<ArcAppMetricsData>(kHistogramBase);
  }

 protected:
  ArcAppMetricsDataTest() = default;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester tester_;
  std::unique_ptr<ArcAppMetricsData> arc_app_metrics_data_;
};

TEST_F(ArcAppMetricsDataTest, DoNotRecordTimeDeltaWhenNoStartExists) {
  arc_app_metrics_data_->maybeReportInstallTimeDelta(kPackageName);
  tester_.ExpectTotalCount(kTestTimeDeltaHistogram, 0);
}

TEST_F(ArcAppMetricsDataTest, DoNotRecordIncompleteWhenNoInstallsRequested) {
  arc_app_metrics_data_->reportMetrics();
  tester_.ExpectTotalCount(kTestAppsIncompleteHistogram, 0);
  tester_.ExpectUniqueSample(kTestAppsRequestedHistogram, 0, 1);
}

TEST_F(ArcAppMetricsDataTest, RecordCorrectTimeDeltaForOnePackage) {
  base::TimeDelta expected_time = base::Minutes(1);
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName);
  task_environment_.AdvanceClock(expected_time);
  arc_app_metrics_data_->maybeReportInstallTimeDelta(kPackageName);
  tester_.ExpectUniqueTimeSample(kTestTimeDeltaHistogram, expected_time, 1);
}

TEST_F(ArcAppMetricsDataTest, RecordCorrectTimeDeltaForTwoPackages) {
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName);
  task_environment_.AdvanceClock(base::Minutes(1));
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName2);
  task_environment_.AdvanceClock(base::Minutes(1));

  arc_app_metrics_data_->maybeReportInstallTimeDelta(kPackageName);
  arc_app_metrics_data_->maybeReportInstallTimeDelta(kPackageName2);

  tester_.ExpectTotalCount(kTestTimeDeltaHistogram, 2);
  tester_.ExpectTimeBucketCount(kTestTimeDeltaHistogram, base::Minutes(1), 1);
  tester_.ExpectTimeBucketCount(kTestTimeDeltaHistogram, base::Minutes(2), 1);
}

TEST_F(ArcAppMetricsDataTest, RecordZeroIncompleteInstalls) {
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName);
  arc_app_metrics_data_->maybeReportInstallTimeDelta(kPackageName);
  arc_app_metrics_data_->reportMetrics();

  tester_.ExpectUniqueSample(kTestAppsIncompleteHistogram, 0, 1);
  tester_.ExpectUniqueSample(kTestAppsRequestedHistogram, 1, 1);
}

TEST_F(ArcAppMetricsDataTest, RecordOneIncompleteInstall) {
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName);
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName2);
  arc_app_metrics_data_->maybeReportInstallTimeDelta(kPackageName);
  arc_app_metrics_data_->reportMetrics();

  tester_.ExpectUniqueSample(kTestAppsIncompleteHistogram, 1, 1);
  tester_.ExpectUniqueSample(kTestAppsRequestedHistogram, 2, 1);
}

TEST_F(ArcAppMetricsDataTest, RecordTwoIncompleteInstalls) {
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName);
  arc_app_metrics_data_->recordAppInstallStartTime(kPackageName2);
  arc_app_metrics_data_->reportMetrics();

  tester_.ExpectUniqueSample(kTestAppsIncompleteHistogram, 2, 1);
  tester_.ExpectUniqueSample(kTestAppsRequestedHistogram, 2, 1);
}

}  // namespace arc
