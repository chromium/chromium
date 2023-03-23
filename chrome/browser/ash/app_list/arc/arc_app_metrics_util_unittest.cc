// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_metrics_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";
constexpr char kManualInstallTimeDeltaHistogram[] =
    "Arc.AppInstall.Manual.TimeDelta";
constexpr char kManualInstallIncompleteHistogram[] =
    "Arc.AppInstall.Manual.NumAppsIncomplete";

}  // namespace

namespace arc {

class ArcAppMetricsUtilTest : public testing::Test {
 public:
  ArcAppMetricsUtilTest(const ArcAppMetricsUtilTest&) = delete;
  ArcAppMetricsUtilTest& operator=(const ArcAppMetricsUtilTest&) = delete;

 protected:
  ArcAppMetricsUtilTest() = default;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester tester_;
  ArcAppMetricsUtil arc_app_metrics_util_;
};

TEST_F(ArcAppMetricsUtilTest, DoNotRecordTimeDeltaWhenNoStartExists) {
  arc_app_metrics_util_.maybeReportInstallTimeDelta(kPackageName);
  tester_.ExpectTotalCount(kManualInstallTimeDeltaHistogram, 0);
}

TEST_F(ArcAppMetricsUtilTest, RecordCorrectTimeDeltaForOnePackage) {
  base::TimeDelta expected_time = base::Minutes(1);
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName);
  task_environment_.AdvanceClock(expected_time);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(kPackageName);
  tester_.ExpectUniqueTimeSample(kManualInstallTimeDeltaHistogram,
                                 expected_time, 1);
}

TEST_F(ArcAppMetricsUtilTest, RecordCorrectTimeDeltaForTwoPackages) {
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName);
  task_environment_.AdvanceClock(base::Minutes(1));
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName2);
  task_environment_.AdvanceClock(base::Minutes(1));

  arc_app_metrics_util_.maybeReportInstallTimeDelta(kPackageName);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(kPackageName2);

  tester_.ExpectTotalCount(kManualInstallTimeDeltaHistogram, 2);
  tester_.ExpectTimeBucketCount(kManualInstallTimeDeltaHistogram,
                                base::Minutes(1), 1);
  tester_.ExpectTimeBucketCount(kManualInstallTimeDeltaHistogram,
                                base::Minutes(2), 1);
}

TEST_F(ArcAppMetricsUtilTest, RecordZeroIncompleteInstalls) {
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(kPackageName);
  arc_app_metrics_util_.reportIncompleteInstalls();

  tester_.ExpectUniqueSample(kManualInstallIncompleteHistogram, 0, 1);
}

TEST_F(ArcAppMetricsUtilTest, RecordOneIncompleteInstall) {
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName);
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName2);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(kPackageName);
  arc_app_metrics_util_.reportIncompleteInstalls();

  tester_.ExpectUniqueSample(kManualInstallIncompleteHistogram, 1, 1);
}

TEST_F(ArcAppMetricsUtilTest, RecordTwoIncompleteInstalls) {
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName);
  arc_app_metrics_util_.recordAppInstallStartTime(kPackageName2);
  arc_app_metrics_util_.reportIncompleteInstalls();

  tester_.ExpectUniqueSample(kManualInstallIncompleteHistogram, 2, 1);
}

}  // namespace arc
