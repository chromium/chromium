// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_performance_test_base.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/tracing.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

namespace {

// Number of events to trim from the beginning and end. These events don't
// contribute anything toward stable measurements: A brief moment of startup
// "jank" is acceptable, and shutdown may result in missing events (since
// render widget draws may stop before capture stops).
constexpr int kTrimEvents = 24;  // 1 sec at 24fps, or 0.4 sec at 60 fps.

// Minimum number of events required for a reasonable analysis.
constexpr int kMinDataPointsForFullRun = 100;  // ~5 sec at 24fps.

// Minimum number of events required for data analysis in a non-performance run.
constexpr int kMinDataPointsForQuickRun = 3;

constexpr char kMetricPrefixTabCapture[] = "TabCapture.";
constexpr char kMetricCaptureMs[] = "capture";
constexpr char kMetricCaptureFailRatePercent[] = "capture_fail_rate";
constexpr char kMetricCaptureLatencyMs[] = "capture_latency";
constexpr char kMetricRendererFrameDrawMs[] = "renderer_frame_draw";

constexpr char kEventCapture[] = "Capture";
constexpr char kEventSuffixFailRate[] = "FailRate";
constexpr char kEventSuffixLatency[] = "Latency";
constexpr char kEventCommitAndDrawCompositorFrame[] =
    "WidgetBase::DidCommitAndDrawCompositorFrame";
const base::flat_map<std::string, std::string> kEventToMetricMap(
    {{kEventCapture, kMetricCaptureMs},
     {std::string(kEventCapture) + kEventSuffixFailRate,
      kMetricCaptureFailRatePercent},
     {std::string(kEventCapture) + kEventSuffixLatency,
      kMetricCaptureLatencyMs},
     {kEventCommitAndDrawCompositorFrame, kMetricRendererFrameDrawMs}});

perf_test::PerfResultReporter SetUpTabCaptureReporter(
    const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixTabCapture, story);
  reporter.RegisterImportantMetric(kMetricCaptureMs, "ms");
  reporter.RegisterImportantMetric(kMetricCaptureFailRatePercent, "percent");
  reporter.RegisterImportantMetric(kMetricCaptureLatencyMs, "ms");
  reporter.RegisterImportantMetric(kMetricRendererFrameDrawMs, "ms");
  return reporter;
}

std::string GetMetricFromEventName(const std::string& event_name) {
  auto iter = kEventToMetricMap.find(event_name);
  return iter == kEventToMetricMap.end() ? event_name : iter->second;
}

// A convenience macro to run a gtest expectation in the "full performance run"
// setting, or else a warning that something is not being entirely tested in the
// "CQ run" setting. This is required because the test runs in the CQ may not be
// long enough to collect sufficient tracing data; and, unfortunately, there's
// nothing we can do about that.
#define EXPECT_FOR_PERFORMANCE_RUN(expr)             \
  do {                                               \
    if (is_full_performance_run()) {                 \
      EXPECT_TRUE(expr);                             \
    } else if (!(expr)) {                            \
      LOG(WARNING) << "Allowing failure: " << #expr; \
    }                                                \
  } while (false)

enum TestFlags {
  kUseGpu = 1 << 0,              // Only execute test if --enable-gpu was given
                                 // on the command line.  This is required for
                                 // tests that run on GPU.
  kTestThroughWebRTC = 1 << 3,   // Send video through a webrtc loopback.
  kSmallWindow = 1 << 4,         // Window size: 1 = 800x600, 0 = 2000x1000
};

// Perfetto trace events should have a "success" that is either on
// the beginning or end event.
bool EventWasSuccessful(const trace_analyzer::TraceEvent* event) {
  double result;
  // First case: the begin event had a success.
  if (event->GetArgAsNumber("success", &result) && result > 0.0) {
    return true;
  }

  // Second case: the end event had a success.
  if (event->other_event &&
      event->other_event->GetArgAsNumber("success", &result) && result > 0.0) {
    return true;
  }

  return false;
}
class TabCapturePerformanceTest : public TabCapturePerformanceTestBase,
                                  public testing::WithParamInterface<int> {
 public:
  TabCapturePerformanceTest() = default;
  ~TabCapturePerformanceTest() override = default;

  bool HasFlag(TestFlags flag) const {
    return (GetParam() & flag) == flag;
  }

  std::string GetSuffixForTestFlags() const {
    std::string suffix;
    if (HasFlag(kUseGpu))
      suffix += "_comp_gpu";
    if (HasFlag(kTestThroughWebRTC))
      suffix += "_webrtc";
    if (HasFlag(kSmallWindow))
      suffix += "_small";
    // Make sure we always have a story.
    if (suffix.size() == 0) {
      suffix = "_baseline_story";
    }
    // Strip off the leading _.
    suffix.erase(0, 1);
    return suffix;
  }

  void SetUp() override {
    const base::FilePath test_file = GetApiTestDataDir()
                                         .AppendASCII("tab_capture")
                                         .AppendASCII("balls.html");
    const bool success = base::ReadFileToString(test_file, &test_page_html_);
    CHECK(success) << "Failed to load test page at: "
                   << test_file.AsUTF8Unsafe();

    if (!HasFlag(kUseGpu))
      UseSoftwareCompositing();

    TabCapturePerformanceTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (HasFlag(kSmallWindow)) {
      command_line->AppendSwitchASCII(switches::kWindowSize, "800,600");
    } else {
      command_line->AppendSwitchASCII(switches::kWindowSize, "2000,1500");
    }

    TabCapturePerformanceTestBase::SetUpCommandLine(command_line);
  }

  // Analyze and print the mean and stddev of how often events having the name
  // |event_name| occur.
  bool PrintRateResults(trace_analyzer::TraceAnalyzer* analyzer,
                        const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(analyzer, event_name, &events);

    // Ignore some events for startup/setup/caching/teardown.
    const int trim_count = is_full_performance_run() ? kTrimEvents : 0;
    if (static_cast<int>(events.size()) < trim_count * 2) {
      LOG(ERROR) << "Fewer events for " << event_name
                 << " than would be trimmed: " << events.size();
      return false;
    }
    trace_analyzer::TraceEventVector rate_events(events.begin() + trim_count,
                                                 events.end() - trim_count);
    trace_analyzer::RateStats stats;
    if (!GetRateStats(rate_events, &stats, nullptr)) {
      return false;
    }

    double mean_ms = stats.mean_us / 1000.0;
    double std_dev_ms = stats.standard_deviation_us / 1000.0;
    std::string mean_and_error = base::StringPrintf("%f,%f", mean_ms,
                                                    std_dev_ms);
    auto reporter = SetUpTabCaptureReporter(GetSuffixForTestFlags());
    reporter.AddResultMeanAndError(GetMetricFromEventName(event_name),
                                   mean_and_error);
    return true;
  }

  // Analyze and print the mean and stddev of the amount of time between the
  // begin and end timestamps of each event having the name |event_name|.
  bool PrintLatencyResults(trace_analyzer::TraceAnalyzer* analyzer,
                           const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(analyzer, event_name, &events);

    // Ignore some events for startup/setup/caching/teardown.
    const int trim_count = is_full_performance_run() ? kTrimEvents : 0;
    if (static_cast<int>(events.size()) < trim_count * 2) {
      LOG(ERROR) << "Fewer events for " << event_name
                 << " than would be trimmed: " << events.size();
      return false;
    }
    trace_analyzer::TraceEventVector events_to_analyze(
        events.begin() + trim_count, events.end() - trim_count);

    // Compute mean and standard deviation of all capture latencies.
    double sum = 0.0;
    double sqr_sum = 0.0;
    int count = 0;
    for (const auto* begin_event : events_to_analyze) {
      const auto* end_event = begin_event->other_event.get();
      if (!end_event)
        continue;
      const double latency = end_event->timestamp - begin_event->timestamp;
      sum += latency;
      sqr_sum += latency * latency;
      ++count;
    }
    const double mean_us = (count == 0) ? NAN : (sum / count);
    const double std_dev_us =
        (count == 0)
            ? NAN
            : (sqrt(std::max(0.0, count * sqr_sum - sum * sum)) / count);
    auto reporter = SetUpTabCaptureReporter(GetSuffixForTestFlags());
    reporter.AddResultMeanAndError(
        GetMetricFromEventName(event_name + kEventSuffixLatency),
        base::StringPrintf("%f,%f", mean_us / 1000.0, std_dev_us / 1000.0));
    return count > 0;
  }

  // Analyze and print the mean and stddev of how often events having the name
  // |event_name| are missing the success=true flag.
  bool PrintFailRateResults(trace_analyzer::TraceAnalyzer* analyzer,
                            const std::string& event_name) {
    trace_analyzer::TraceEventVector events;
    QueryTraceEvents(analyzer, event_name, &events);

    // Ignore some events for startup/setup/caching/teardown.
    const int trim_count = is_full_performance_run() ? kTrimEvents : 0;
    if (static_cast<int>(events.size()) < trim_count * 2) {
      LOG(ERROR) << "Fewer events for " << event_name
                 << " than would be trimmed: " << events.size();
      return false;
    }
    trace_analyzer::TraceEventVector events_to_analyze(
        events.begin() + trim_count, events.end() - trim_count);

    // Compute percentage of begin→end events missing a success=true flag.
    // If there are no events to analyze, then the failure rate is 100%.
    double fail_percent = 100.0;
    if (!events_to_analyze.empty()) {
      int fail_count = 0;
      for (const auto* event : events_to_analyze) {
        if (!EventWasSuccessful(event)) {
          ++fail_count;
        }
      }
      fail_percent = 100.0 * static_cast<double>(fail_count) /
                     static_cast<double>(events_to_analyze.size());
    }
    auto reporter = SetUpTabCaptureReporter(GetSuffixForTestFlags());
    reporter.AddResult(
        GetMetricFromEventName(event_name + kEventSuffixFailRate),
        fail_percent);
    return !events_to_analyze.empty();
  }

 protected:
  // The HTML test web page that draws animating balls continuously. Populated
  // in SetUp().
  std::string test_page_html_;
};

}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
// Using MSAN on ChromeOS causes problems due to its hardware OpenGL library.
// Failing on ChromeOS Lacros as well.
#define MAYBE_Performance DISABLED_Performance
#elif BUILDFLAG(IS_MAC)
// TODO(crbug.com/1235358): Flaky on Mac 10.11
#define MAYBE_Performance DISABLED_Performance
#elif BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
// TODO(crbug.com/40214499): Flaky on Linux ASAN
#define MAYBE_Performance DISABLED_Performance
#else
#define MAYBE_Performance Performance
#endif
IN_PROC_BROWSER_TEST_P(TabCapturePerformanceTest, MAYBE_Performance) {
  // Load the extension and test page, and tell the extension to start tab
  // capture.
  LoadExtension(GetApiTestDataDir()
                    .AppendASCII("tab_capture")
                    .AppendASCII("perftest_extension"));
  NavigateToTestPage(test_page_html_);
  const base::Value response = SendMessageToExtension(
      base::StringPrintf("{start:true, passThroughWebRTC:%s}",
                         HasFlag(kTestThroughWebRTC) ? "true" : "false"));
  ASSERT_TRUE(response.is_dict());
  const std::string* reason = response.GetDict().FindString("reason");
  ASSERT_TRUE(response.GetDict().FindBool("success").value_or(false))
      << (reason ? *reason : std::string("<MISSING REASON>"));

  // Observe the running browser for a while, collecting a trace.
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer = TraceAndObserve(
      "gpu,gpu.capture",
      std::vector<std::string_view>{kEventCommitAndDrawCompositorFrame,
                                    kEventCapture},
      // In a full performance run, events will be trimmed from both ends of
      // trace. Otherwise, just require the bare-minimum to verify the stats
      // calculations will work.
      is_full_performance_run() ? (2 * kTrimEvents + kMinDataPointsForFullRun)
                                : kMinDataPointsForQuickRun);

  // The printed result will be the average time between composites in the
  // renderer of the page being captured. This may not reach the full frame
  // rate if the renderer cannot draw as fast as is desired.
  //
  // Note that any changes to drawing or compositing in the renderer,
  // including changes to Blink (e.g., Canvas drawing), layout, etc.; will
  // have an impact on this result.
  EXPECT_FOR_PERFORMANCE_RUN(
      PrintRateResults(analyzer.get(), kEventCommitAndDrawCompositorFrame));

  // This prints out the average time between capture events in the browser
  // process. This should roughly match the renderer's draw+composite rate.
  EXPECT_FOR_PERFORMANCE_RUN(PrintRateResults(analyzer.get(), kEventCapture));

  // Analyze mean/stddev of the capture latency. This is a measure of how long
  // each capture took, from initiation until read-back from the GPU into a
  // media::VideoFrame was complete. Lower is better.
  EXPECT_FOR_PERFORMANCE_RUN(
      PrintLatencyResults(analyzer.get(), kEventCapture));

  // Analyze percentage of failed captures. This measures how often captures
  // were initiated, but not completed successfully. Lower is better, and zero
  // is ideal.
  EXPECT_FOR_PERFORMANCE_RUN(
      PrintFailRateResults(analyzer.get(), kEventCapture));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// On ChromeOS, software compositing is not an option.
INSTANTIATE_TEST_SUITE_P(All,
                         TabCapturePerformanceTest,
                         testing::Values(kUseGpu,
                                         kTestThroughWebRTC | kUseGpu));

#else

// Run everything on non-ChromeOS platforms.
INSTANTIATE_TEST_SUITE_P(All,
                         TabCapturePerformanceTest,
                         testing::Values(0,
                                         kUseGpu,
                                         kTestThroughWebRTC,
                                         kTestThroughWebRTC | kUseGpu));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
