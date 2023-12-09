// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_sync_metrics_helper.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kLatencyHistogramName[] = "Arc.AppSync.InitialSession.Latency";
constexpr char kExpectedAppHistogramName[] =
    "Arc.AppSync.InitialSession.NumAppsExpected";
constexpr char kInstalledAppHistogramName[] =
    "Arc.AppSync.InitialSession.NumAppsInstalled";
constexpr char kNotInstalledAppHistogramName[] =
    "Arc.AppSync.InitialSession.NumAppsNotInstalled";
constexpr char kInstalledAppSizeHistogramName[] =
    "Arc.AppSync.InitialSession.InstalledAppSize";

constexpr uint64_t kTestAppSizeInBytes = 2048000;
constexpr uint64_t kTestAppSizeInMB = kTestAppSizeInBytes / (1000 * 1000);
constexpr std::optional<uint64_t> kTestAppSizeOptional =
    std::optional<uint64_t>{kTestAppSizeInBytes};

}  // namespace

namespace arc {

class ArcAppSyncMetricsHelperTest : public testing::Test {
 public:
  ArcAppSyncMetricsHelperTest(const ArcAppSyncMetricsHelperTest&) = delete;
  ArcAppSyncMetricsHelperTest& operator=(const ArcAppSyncMetricsHelperTest&) =
      delete;

 protected:
  ArcAppSyncMetricsHelperTest() = default;

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ArcAppSyncMetricsHelper metrics_helper_;
  base::HistogramTester tester_;
};

TEST_F(ArcAppSyncMetricsHelperTest,
       OneAppExpectedAndNotDownloadedThenDontRecordLatency) {
  metrics_helper_.SetTimeSyncStarted();
  metrics_helper_.SetAndRecordNumExpectedApps(1);
  metrics_helper_.RecordMetrics();

  tester_.ExpectUniqueSample(kNotInstalledAppHistogramName, 1, 1);
  tester_.ExpectUniqueSample(kInstalledAppHistogramName, 0, 1);
  tester_.ExpectUniqueSample(kExpectedAppHistogramName, 1, 1);
  tester_.ExpectTotalCount(kLatencyHistogramName, 0);
}

TEST_F(ArcAppSyncMetricsHelperTest, OneAppExpectedAndDownloaded) {
  base::TimeDelta expected_latency = base::Minutes(1);
  metrics_helper_.SetTimeSyncStarted();
  metrics_helper_.SetAndRecordNumExpectedApps(1);
  task_environment.AdvanceClock(expected_latency);
  metrics_helper_.OnAppInstalled(kTestAppSizeOptional);
  metrics_helper_.RecordMetrics();

  tester_.ExpectUniqueSample(kNotInstalledAppHistogramName, 0, 1);
  tester_.ExpectUniqueSample(kInstalledAppHistogramName, 1, 1);
  tester_.ExpectUniqueSample(kExpectedAppHistogramName, 1, 1);
  tester_.ExpectUniqueSample(kLatencyHistogramName,
                             expected_latency.InSeconds(), 1);
}

TEST_F(ArcAppSyncMetricsHelperTest, TwoAppsExpectedAndOneDownloaded) {
  base::TimeDelta expected_latency = base::Minutes(5);
  metrics_helper_.SetTimeSyncStarted();
  metrics_helper_.SetAndRecordNumExpectedApps(2);
  task_environment.AdvanceClock(expected_latency);
  metrics_helper_.OnAppInstalled(kTestAppSizeOptional);
  metrics_helper_.RecordMetrics();

  tester_.ExpectUniqueSample(kNotInstalledAppHistogramName, 1, 1);
  tester_.ExpectUniqueSample(kInstalledAppHistogramName, 1, 1);
  tester_.ExpectUniqueSample(kExpectedAppHistogramName, 2, 1);
  tester_.ExpectUniqueSample(kLatencyHistogramName,
                             expected_latency.InSeconds(), 1);
}

TEST_F(ArcAppSyncMetricsHelperTest,
       ThreeAppsExpectedAndDownloadedThenRecordTotalLatency) {
  int32_t num_expected_apps = 3;
  base::TimeDelta latency_per_app = base::Minutes(1);
  metrics_helper_.SetTimeSyncStarted();
  metrics_helper_.SetAndRecordNumExpectedApps(num_expected_apps);
  for (int i = 0; i < num_expected_apps; i++) {
    task_environment.AdvanceClock(latency_per_app);
    metrics_helper_.OnAppInstalled(kTestAppSizeOptional);
  }
  metrics_helper_.RecordMetrics();
  base::TimeDelta expected_latency = latency_per_app * num_expected_apps;

  tester_.ExpectUniqueSample(kNotInstalledAppHistogramName, 0, 1);
  tester_.ExpectUniqueSample(kInstalledAppHistogramName, 3, 1);
  tester_.ExpectUniqueSample(kExpectedAppHistogramName, 3, 1);
  tester_.ExpectUniqueSample(kLatencyHistogramName,
                             expected_latency.InSeconds(), 1);
}

TEST_F(ArcAppSyncMetricsHelperTest, DoNotRecordInstalledAppSize) {
  metrics_helper_.OnAppInstalled(std::optional<uint64_t>());
  tester_.ExpectTotalCount(kInstalledAppSizeHistogramName, 0);
}

TEST_F(ArcAppSyncMetricsHelperTest, RecordInstalledAppSizeForOneApp) {
  metrics_helper_.OnAppInstalled(kTestAppSizeOptional);
  tester_.ExpectUniqueSample(kInstalledAppSizeHistogramName, kTestAppSizeInMB,
                             1);
}

TEST_F(ArcAppSyncMetricsHelperTest, RecordInstalledAppSizeForTwoApps) {
  const uint64_t test_app_size_bytes = 2048000000;
  metrics_helper_.OnAppInstalled(kTestAppSizeOptional);
  tester_.ExpectUniqueSample(kInstalledAppSizeHistogramName, kTestAppSizeInMB,
                             1);

  metrics_helper_.OnAppInstalled(std::optional<uint64_t>{test_app_size_bytes});
  tester_.ExpectBucketCount(kInstalledAppSizeHistogramName,
                            test_app_size_bytes / (1000 * 1000), 1);
  tester_.ExpectTotalCount(kInstalledAppSizeHistogramName, 2);
}

}  // namespace arc
