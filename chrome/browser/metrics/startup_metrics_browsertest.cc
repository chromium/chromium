// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    "Startup.BrowserMessageLoopStartHardFaultBytes",
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
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)
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
  if (base::android::android_info::sdk_int() >=
      base::android::android_info::SDK_VERSION_NOUGAT) {
    AddProcessCreateMetrics(startup_metrics);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Wait for all histograms to be recorded. The test will hit a RunLoop timeout
  // if a histogram is not recorded.
  for (auto* const histogram : startup_metrics) {
    SCOPED_TRACE(histogram);

    // Continue if histograms was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram)) {
      continue;
    }

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram, run_loop.QuitClosure());
    run_loop.Run();
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr char kFirstContentfulPaintHistogram[] =
    "Startup.FirstWebContents.FirstContentfulPaint";
constexpr char kLargestContentfulPaintHistogram[] =
    "Startup.FirstWebContents.LargestContentfulPaint";

// A startup page with visible content so that the first web contents produces a
// first contentful paint and a largest contentful paint candidate. The default
// browser-test startup page (about:blank) produces a visually-non-empty paint
// but neither FCP nor LCP.
constexpr char kContentfulStartupUrl[] =
    "data:text/html,<h1>StartupContentfulPaintTest</h1>";

// Waits for the next OnLargestContentfulPaintInPrimaryMainFrame() notification
// on the observed WebContents.
class LargestContentfulPaintObserver : public content::WebContentsObserver {
 public:
  explicit LargestContentfulPaintObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void Wait() { run_loop_.Run(); }

 private:
  // content::WebContentsObserver:
  void OnLargestContentfulPaintInPrimaryMainFrame(
      base::TimeTicks presentation_time) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

// Blocks until a sample is recorded for `histogram`.
void WaitForHistogramSample(std::string_view histogram) {
  if (base::StatisticsRecorder::FindHistogram(histogram)) {
    return;
  }
  base::RunLoop run_loop;
  base::StatisticsRecorder::ScopedHistogramSampleObserver observer(
      histogram, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

// Launches the browser with a contentful page as the first web contents so that
// the FirstWebContentsProfiler can observe paint timing metrics (the default
// about:blank startup page produces neither FCP nor LCP).
class StartupMetricsContentfulPaintTest : public PlatformBrowserTest {
 public:
  StartupMetricsContentfulPaintTest() {
    // Prevent InProcessBrowserTest from forcing about:blank as the startup tab
    // so that the contentful URL appended below becomes the first web contents.
    set_open_about_blank_on_browser_launch(false);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendArg(kContentfulStartupUrl);
  }
};

// Verifies that the startup first contentful paint and largest contentful paint
// histograms are recorded end-to-end for the first web contents.
IN_PROC_BROWSER_TEST_F(StartupMetricsContentfulPaintTest,
                       RecordsFirstAndLargestContentfulPaint) {
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // The first contentful paint is recorded during startup once the contentful
  // startup page paints.
  WaitForHistogramSample(kFirstContentfulPaintHistogram);

  // Paint a larger element so the profiler deterministically observes a largest
  // contentful paint candidate, then wait for that notification.
  LargestContentfulPaintObserver lcp_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(web_contents, R"(
      const element = document.createElement('div');
      element.style.fontSize = '160px';
      element.textContent = 'LARGER';
      document.body.appendChild(element);
  )"));
  lcp_observer.Wait();

  // Navigating away finalizes the profiler, which records the largest
  // contentful paint observed for the first web contents.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));

  WaitForHistogramSample(kLargestContentfulPaintHistogram);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
