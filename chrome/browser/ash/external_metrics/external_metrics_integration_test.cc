// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/external_metrics/external_metrics.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"

namespace ash {
namespace {

// The metrics_client binary on ChromeOS incorporates libmetrics and allows us
// to report metrics via its command line. This binary is normally in /usr/bin
// and should be on the path so we don't qualify the directory.
const char kMetricsClientBinaryName[] = "metrics_client";

// Wait up to this long for the metrics client to complete running.
constexpr int kMetricsClientTimeoutSec = 30;

const char kTestHistogramName[] = "ExternalMetricsIntegrationTest.Histogram";

class ExternalMetricsIntegrationTest : public AshIntegrationTest {
 public:
  void ReportMetrics(int num_samples, int value, int max) {
    std::vector<std::string> args{kMetricsClientBinaryName};
    if (num_samples > 1) {
      args.push_back("-n");
      args.push_back(base::NumberToString(num_samples));
    }
    args.push_back(kTestHistogramName);
    args.push_back("-e");
    args.push_back(base::NumberToString(value));
    args.push_back(base::NumberToString(max));

    base::LaunchOptions launch_opts;
    base::Process metrics_client = base::LaunchProcess(args, launch_opts);
    ASSERT_TRUE(metrics_client.IsValid());

    int exit_code = 0;
    ASSERT_TRUE(metrics_client.WaitForExitWithTimeout(
        base::Seconds(kMetricsClientTimeoutSec), &exit_code));
    ASSERT_EQ(0, exit_code);
  }

  void SyncHistograms() {
    ExternalMetrics* metrics = ExternalMetrics::Get();
    ASSERT_TRUE(metrics) << "BrowserMain should have set up ExternalMetrics";

    metrics->CollectNowForTesting();

    // ExternalMetrics above will have pushed the histograms into the metrics
    // system so they should be ready to use. If we need metrics from child
    // processes like renderers, we would have to call
    // FetchHistogramsAsynchronously() and
    // base::StatisticsRecorder::ImportProvidedHistogramsSync() here.
  }
};

}  // namespace

// Integration test between histograms reported by libmetrics in ChromeOS and
// Ash's importer of these metrics.
IN_PROC_BROWSER_TEST_F(ExternalMetricsIntegrationTest, SyncHistograms) {
  constexpr int kSample1 = 1;
  constexpr int kCount1 = 1;

  constexpr int kSample2 = 2;
  constexpr int kCount2 = 10;

  constexpr int kMax = 5;

  // Must be created first because this class tracks *changes* in histograms.
  base::HistogramTester tester;

  ReportMetrics(kCount1, kSample1, kMax);
  SyncHistograms();
  tester.ExpectBucketCount(kTestHistogramName, kSample1, kCount1);
  // Shouldn't have any "sample 2" yet.
  tester.ExpectBucketCount(kTestHistogramName, kSample2, 0);

  ReportMetrics(kCount2, kSample2, kMax);
  SyncHistograms();
  tester.ExpectBucketCount(kTestHistogramName, kSample2, kCount2);
}

}  // namespace ash
