// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/task_info_metrics_provider_mac.h"

#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TaskInfoMetricsProviderMacTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(TaskInfoMetricsProviderMacTest, RecordsBrowserProcessHistograms) {
  TaskInfoMetricsProviderMac provider;
  provider.OnRecordingEnabled();

  static constexpr const char* kCounters[] = {
      "PageFaultsPerSec",      "MajorPageFaultsPerSec", "CowFaultsPerSec",
      "ContextSwitchesPerSec", "VirtualSize",           "ResidentSize",
      "ResidentVirtualRatio",  "ThreadCount",           "RunningThreadCount",
  };

  auto expect_counts = [&](int first_sample_count, int steady_count) {
    for (const char* counter : kCounters) {
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Mac.Experimental.Process.", counter, ".Browser.FirstSample"}),
          first_sample_count);
      histogram_tester_.ExpectTotalCount(
          base::StrCat({"Mac.Experimental.Process.", counter, ".Browser"}),
          steady_count);
    }
  };

  // First sample (at t=30s) establishes baseline without emitting any
  // histograms.
  environment_.FastForwardBy(base::Seconds(30));
  expect_counts(0, 0);

  // Second sample (at t=60s) computes delta and emits only the .FirstSample
  // histograms (non-first-sample histograms remain 0).
  environment_.FastForwardBy(base::Seconds(30));
  expect_counts(1, 0);

  // Third sample (at t=90s) emits steady-state non-first-sample histograms for
  // the first time, while .FirstSample remains at 1.
  environment_.FastForwardBy(base::Seconds(30));
  expect_counts(1, 1);

  // Fourth sample (at t=120s) increments steady-state histograms to 2, while
  // .FirstSample stays exactly 1.
  environment_.FastForwardBy(base::Seconds(30));
  expect_counts(1, 2);
}

TEST_F(TaskInfoMetricsProviderMacTest, RecordsRendererProcessHistograms) {
  TaskInfoMetricsProviderMac provider;
  provider.OnRecordingEnabled();

  // Explicitly listen to a simulated Renderer process backed by our PID so
  // proc_pidinfo succeeds.
  provider.GetQueryHandlerForTesting()
      .AsyncCall(
          &TaskInfoMetricsProviderMac::QueryHandler::StartListeningToProcess)
      .WithArgs(content::ChildProcessId(1), base::GetCurrentProcId(),
                "Renderer");

  static constexpr const char* kCounters[] = {
      "PageFaultsPerSec",      "MajorPageFaultsPerSec", "CowFaultsPerSec",
      "ContextSwitchesPerSec", "VirtualSize",           "ResidentSize",
      "ResidentVirtualRatio",  "ThreadCount",           "RunningThreadCount",
  };

  auto expect_renderer_counts = [&](int first_sample_count, int steady_count) {
    for (const char* counter : kCounters) {
      histogram_tester_.ExpectTotalCount(
          base::StrCat(
              {"Mac.Experimental.Process.", counter, ".Renderer.FirstSample"}),
          first_sample_count);
      histogram_tester_.ExpectTotalCount(
          base::StrCat({"Mac.Experimental.Process.", counter, ".Renderer"}),
          steady_count);
    }
  };

  // First sample (at t=30s) establishes baseline without emitting any
  // histograms.
  environment_.FastForwardBy(base::Seconds(30));
  expect_renderer_counts(0, 0);

  // Second sample (at t=60s) emits only .FirstSample for Renderer.
  environment_.FastForwardBy(base::Seconds(30));
  expect_renderer_counts(1, 0);

  // Third sample (at t=90s) emits steady-state non-first-sample histograms.
  environment_.FastForwardBy(base::Seconds(30));
  expect_renderer_counts(1, 1);

  // Fourth sample (at t=120s) increments steady-state histograms to 2.
  environment_.FastForwardBy(base::Seconds(30));
  expect_renderer_counts(1, 2);
}
