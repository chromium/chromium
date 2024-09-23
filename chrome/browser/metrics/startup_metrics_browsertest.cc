// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using StartupMetricsTest = PlatformBrowserTest;

namespace {

constexpr const char* kStartupMetrics[] = {
    "Startup.BrowserMessageLoopFirstIdle",
    "Startup.BrowserMessageLoopStartTime",

// Desktop specific metrics
#if !BUILDFLAG(IS_ANDROID)
    "Startup.BrowserMessageLoopStart.To.NonEmptyPaint2",
    "Startup.BrowserWindow.FirstPaint",
    "Startup.BrowserWindowDisplay",
    "Startup.FirstWebContents.MainNavigationFinished",
    "Startup.FirstWebContents.MainNavigationStart",
    "Startup.FirstWebContents.NonEmptyPaint3",
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    "Startup.BrowserMessageLoopStartHardFaultCount",
    "Startup.Temperature",
#endif
};

void AddProcessCreateMetrics(std::vector<const char*>& v) {
  v.push_back("Startup.LoadTime.ProcessCreateToApplicationStart");
  v.push_back("Startup.LoadTime.ApplicationStartToChromeMain");
}

}  // namespace

// Verify that startup histograms are logged on browser startup.
// TODO(crbug.com/40919406): Re-enable this test
// TODO(b/321634178): Disable the test on Lacros due to flakiness.
#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_ReportsValues DISABLED_ReportsValues
#else
#define MAYBE_ReportsValues ReportsValues
#endif
IN_PROC_BROWSER_TEST_F(StartupMetricsTest, MAYBE_ReportsValues) {
  std::vector<const char*> startup_metrics{std::begin(kStartupMetrics),
                                           std::end(kStartupMetrics)};

#if !BUILDFLAG(IS_ANDROID)
  AddProcessCreateMetrics(startup_metrics);
#else
  // On Android these metrics are based on Process.getStartUptimeMillis() - not
  // available before N.
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_NOUGAT) {
    AddProcessCreateMetrics(startup_metrics);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Wait for all histograms to be recorded. The test will hit a RunLoop timeout
  // if a histogram is not recorded.
  for (auto* const histogram : startup_metrics) {
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
