// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"

using StartupMetricsTest = InProcessBrowserTest;

namespace {

constexpr const char* kStartupMetrics[] = {
    "Startup.BrowserMainToRendererMain",
    "Startup.BrowserMessageLoopStartTime",
    "Startup.BrowserMessageLoopStartTimeFromMainEntry3",
    "Startup.BrowserOpenTabs",
    "Startup.BrowserWindow.FirstPaint",
    "Startup.BrowserWindow.FirstPaint.CompositingEnded",
    "Startup.BrowserWindowDisplay",
    "Startup.FirstWebContents.MainNavigationFinished",
    "Startup.FirstWebContents.MainNavigationStart",
    "Startup.FirstWebContents.NonEmptyPaint2",
    "Startup.FirstWebContents.RenderProcessHostInit.ToNonEmptyPaint",
    "Startup.LoadTime.ExeMainToDllMain2",
    "Startup.LoadTime.ProcessCreateToDllMain2",
    "Startup.LoadTime.ProcessCreateToExeMain2",
    "Startup.SystemUptime",

#if defined(OS_WIN)
    "Startup.BrowserMessageLoopStartHardFaultCount",
    "Startup.Temperature",
#endif
};

}  // namespace

// Verify that startup histograms are logged on browser startup.
IN_PROC_BROWSER_TEST_F(StartupMetricsTest, ReportsValues) {
  // This is usually done from the constructor of ChromeMainDelegate.
  startup_metric_utils::RecordExeMainEntryPointTicks(base::TimeTicks::Now());

  // This is usually done from ChromeBrowserMainParts::MainMessageLoopRun().
  startup_metric_utils::RecordBrowserMainMessageLoopStart(
      base::TimeTicks::Now(), false /* is_first_run */);

  // Wait for all histograms to be recorded. The test will hang if an histogram
  // is not recorded.
  for (auto* const histogram : kStartupMetrics) {
    while (!base::StatisticsRecorder::FindHistogram(histogram))
      base::RunLoop().RunUntilIdle();
  }
}
