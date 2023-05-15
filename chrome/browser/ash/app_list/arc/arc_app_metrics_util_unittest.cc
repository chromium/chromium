// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_metrics_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";

constexpr char kManualTimeDeltaHistogram[] =
    "Arc.AppInstall.Manual.InitialSession.TimeDelta";
constexpr char kManualAppsIncompleteHistogram[] =
    "Arc.AppInstall.Manual.InitialSession.NumAppsIncomplete";
constexpr char kManualAppsRequestedHistogram[] =
    "Arc.AppInstall.Manual.InitialSession.NumAppsRequested";

constexpr char kPolicyTimeDeltaHistogram[] =
    "Arc.AppInstall.Policy.InitialSession.TimeDelta";
constexpr char kPolicyAppsIncompleteHistogram[] =
    "Arc.AppInstall.Policy.InitialSession.NumAppsIncomplete";
constexpr char kPolicyAppsRequestedHistogram[] =
    "Arc.AppInstall.Policy.InitialSession.NumAppsRequested";

}  // namespace

namespace arc {

class ArcAppMetricsUtilTest : public testing::Test {
 public:
  ArcAppMetricsUtilTest(const ArcAppMetricsUtilTest&) = delete;
  ArcAppMetricsUtilTest& operator=(const ArcAppMetricsUtilTest&) = delete;

 protected:
  ArcAppMetricsUtilTest() = default;
  base::HistogramTester tester_;
  ArcAppMetricsUtil arc_app_metrics_util_;
};

TEST_F(ArcAppMetricsUtilTest, RecordTimeAfterPolicyInstall) {
  arc_app_metrics_util_.recordAppInstallStartTime(
      kPackageName,
      /*is_controlled_by_policy=*/true);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(
      kPackageName,
      /*is_controlled_by_policy=*/true);
  tester_.ExpectTotalCount(kPolicyTimeDeltaHistogram, 1);
  tester_.ExpectTotalCount(kManualTimeDeltaHistogram, 0);
}

TEST_F(ArcAppMetricsUtilTest, RecordTimeAfterManualInstall) {
  arc_app_metrics_util_.recordAppInstallStartTime(
      kPackageName,
      /*is_controlled_by_policy=*/false);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(
      kPackageName,
      /*is_controlled_by_policy=*/false);
  tester_.ExpectTotalCount(kPolicyTimeDeltaHistogram, 0);
  tester_.ExpectTotalCount(kManualTimeDeltaHistogram, 1);
}

TEST_F(ArcAppMetricsUtilTest, RecordInstallCountsAfterBothInstalls) {
  arc_app_metrics_util_.recordAppInstallStartTime(
      kPackageName,
      /*is_controlled_by_policy=*/true);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(
      kPackageName,
      /*is_controlled_by_policy=*/true);

  arc_app_metrics_util_.recordAppInstallStartTime(
      kPackageName2,
      /*is_controlled_by_policy=*/false);
  arc_app_metrics_util_.maybeReportInstallTimeDelta(
      kPackageName2,
      /*is_controlled_by_policy=*/false);

  arc_app_metrics_util_.reportMetrics();
  tester_.ExpectTotalCount(kPolicyAppsIncompleteHistogram, 1);
  tester_.ExpectTotalCount(kPolicyAppsRequestedHistogram, 1);
  tester_.ExpectTotalCount(kManualAppsIncompleteHistogram, 1);
  tester_.ExpectTotalCount(kManualAppsRequestedHistogram, 1);
}

}  // namespace arc
