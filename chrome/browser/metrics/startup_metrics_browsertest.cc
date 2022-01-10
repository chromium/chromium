// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

using StartupMetricsTest = PlatformBrowserTest;

namespace {

constexpr const char* kStartupMetrics[] = {
    "Startup.BrowserMessageLoopFirstIdle",
    "Startup.BrowserMessageLoopStartTime",

// Not Desktop specific but flaky on some Android bots.
// TODO(crbug.com/1252126): Figure out why.
#if !BUILDFLAG(IS_ANDROID)
    "Startup.LoadTime.ApplicationStartToChromeMain",
    "Startup.LoadTime.ProcessCreateToApplicationStart",
#endif  // BUILDFLAG(IS_ANDROID)

// Desktop specific metrics
#if !BUILDFLAG(IS_ANDROID)
    "Startup.BrowserWindow.FirstPaint",
    "Startup.BrowserWindowDisplay",
    "Startup.FirstWebContents.MainNavigationFinished",
    "Startup.FirstWebContents.MainNavigationStart",
    "Startup.FirstWebContents.NonEmptyPaint3",
    "Startup.FirstWebContents.RenderProcessHostInit.ToNonEmptyPaint",
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    "Startup.Temperature",
#endif
};

}  // namespace

// Verify that startup histograms are logged on browser startup.
IN_PROC_BROWSER_TEST_F(StartupMetricsTest, ReportsValues) {
  // Wait for all histograms to be recorded. The test will hit a RunLoop timeout
  // if a histogram is not recorded.
  for (auto* const histogram : kStartupMetrics) {
    SCOPED_TRACE(histogram);

    // Continue if histograms was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram))
      continue;

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }
}
