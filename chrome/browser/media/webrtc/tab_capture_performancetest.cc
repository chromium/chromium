// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/execution_fence.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/tracing/trace_event_analyzer.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "gpu/config/gpu_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/gl/gl_switches.h"

namespace {

using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;

// Trace events that the test measures.
constexpr char kEventCapture[] = "Capture";
constexpr char kEventCommitAndDrawCompositorFrame[] =
    "WidgetBase::DidCommitAndDrawCompositorFrame";

// Reported metrics, all under the "TabCapture." prefix.
constexpr char kMetricCaptureMs[] = "capture";
constexpr char kMetricCaptureFailRatePercent[] = "capture_fail_rate";
constexpr char kMetricCaptureLatencyMs[] = "capture_latency";
constexpr char kMetricRendererFrameDrawMs[] = "renderer_frame_draw";

// Path of the test page served from chrome/test/data/ by the embedded test
// server.
constexpr char kTestWebPagePath[] = "/webrtc/balls.html";

// Number of events to trim from each end of a full run. Startup may be janky,
// and shutdown may drop events, since the renderer can stop drawing before
// capture stops. 24 events is 1 sec at 24 fps, or 0.4 sec at 60 fps.
constexpr size_t kTrimEvents = 24;

// Minimum number of capture events after trimming.
constexpr size_t kMinEventsForFullRun = 100;  // About 5 sec at 24 fps.
constexpr size_t kMinEventsForQuickRun = 3;

struct MeanAndStdDev {
  double mean;
  double std_dev;
};

// Returns the population mean and standard deviation of `values`, or nullopt
// if `values` is empty.
std::optional<MeanAndStdDev> ComputeMeanAndStdDev(
    base::span<const double> values) {
  if (values.empty()) {
    return std::nullopt;
  }
  const double count = static_cast<double>(values.size());
  const double mean =
      std::ranges::fold_left(values, 0.0, std::plus<>()) / count;
  const double sum_of_squared_deviations =
      std::ranges::fold_left(values, 0.0, [mean](double sum, double value) {
        return sum + (value - mean) * (value - mean);
      });
  return MeanAndStdDev{mean, std::sqrt(sum_of_squared_deviations / count)};
}

// Returns the time in milliseconds between the begin and end of each event
// that has a matching end event.
std::vector<double> GetDurationsMs(const TraceEventVector& events) {
  std::vector<double> durations_ms;
  for (const TraceEvent* begin : events) {
    if (const TraceEvent* end = begin->other_event.get()) {
      durations_ms.push_back((end->timestamp - begin->timestamp) / 1000.0);
    }
  }
  return durations_ms;
}

// Returns whether a capture event reports success. Capture events carry either
// a "success" flag or a "result" code, where 1 is CaptureResult::kSuccess, on
// either the begin or the end event.
bool EventWasSuccessful(const TraceEvent* event) {
  auto has_success_arg = [](const TraceEvent& e) {
    double value;
    return (e.GetArgAsNumber("success", &value) && value > 0.0) ||
           (e.GetArgAsNumber("result", &value) && value == 1.0);
  };
  return has_success_arg(*event) ||
         (event->other_event && has_success_arg(*event->other_event));
}

// Returns the percentage of `events` that did not succeed, or nullopt if
// `events` is empty.
std::optional<double> ComputeFailRatePercent(const TraceEventVector& events) {
  if (events.empty()) {
    return std::nullopt;
  }
  const auto failures = std::ranges::count_if(
      events,
      [](const TraceEvent* event) { return !EventWasSuccessful(event); });
  return 100.0 * static_cast<double>(failures) /
         static_cast<double>(events.size());
}

// Returns the events named `event_name`, skipping END events so that each
// begin/end pair is counted once.
TraceEventVector QueryTraceEvents(TraceAnalyzer& analyzer,
                                  std::string_view event_name) {
  using trace_analyzer::Query;
  const Query query =
      Query::EventNameIs(std::string(event_name)) &&
      (Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
       Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
       Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN) ||
       Query::EventPhaseIs(TRACE_EVENT_PHASE_FLOW_BEGIN) ||
       Query::EventPhaseIs(TRACE_EVENT_PHASE_INSTANT) ||
       Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE));
  TraceEventVector events;
  analyzer.FindEvents(query, &events);
  return events;
}

// Gzip-compresses `input`, then Base64-encodes it and wraps it at 80 columns,
// so that a trace can be copied out of a bot log.
std::string MakeBase64EncodedGZippedString(const std::string& input) {
  static constexpr size_t kMaxLineWidth = 80;
  std::string gzipped;
  compression::GzipCompress(input, &gzipped);
  const std::string encoded = base::Base64Encode(gzipped);
  std::string wrapped;
  wrapped.reserve(encoded.size() + encoded.size() / kMaxLineWidth + 1);
  for (size_t pos = 0; pos < encoded.size(); pos += kMaxLineWidth) {
    wrapped.append(encoded, pos, kMaxLineWidth).append(1, '\n');
  }
  return wrapped;
}

// Returns whether the GPU process uses a hardware GPU, as opposed to software
// compositing or a software GL driver such as SwiftShader or llvmpipe.
bool IsUsingHardwareGpu(const gpu::GPUInfo& gpu_info) {
  return content::GpuDataManager::GetInstance()
             ->HardwareAccelerationEnabled() &&
         !gpu_info.active_gpu().IsSoftwareRenderer();
}

// Starts capture of the current tab with getDisplayMedia() and plays the
// captured stream in a video element. Resolves to "success".
constexpr char kStartCaptureScript[] = R"(
(async () => {
  const kTimeoutMillis = 30000;
  const withTimeout = (promise, what) => Promise.race([
    promise,
    new Promise((_, reject) => setTimeout(
        () => reject(new Error(what + ' timed out')), kTimeoutMillis)),
  ]);

  const stream = await withTimeout(navigator.mediaDevices.getDisplayMedia({
    video: {width: {max: 1920}, height: {max: 1080}, frameRate: {max: 60}},
    audio: false,
    preferCurrentTab: true,
  }), 'getDisplayMedia()');

  window.receiverVideo = document.createElement('video');
  window.receiverVideo.muted = true;
  window.receiverVideo.srcObject = stream;
  await withTimeout(window.receiverVideo.play(), 'video.play()');
  return 'success';
})()
)";

// Measures the performance of capturing the current tab with
// getDisplayMedia().
//
// Requires a hardware GPU, since that is the configuration that matters to
// users, and skips on bots that fall back to software rendering. A full
// performance run (--full-performance-run) traces for longer and checks that
// every metric is reported.
class TabCapturePerformanceTest : public InProcessBrowserTest {
 public:
  TabCapturePerformanceTest() = default;
  ~TabCapturePerformanceTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
#if defined(MEMORY_SANITIZER)
    // MSan and hardware GL do not get along, and software GL under MSan is too
    // slow for this test.
    GTEST_SKIP() << "Skipping test under MemorySanitizer.";
#else
    // Capture reads back rendered pixels.
    EnablePixelOutput();
    InProcessBrowserTest::SetUp();
#endif
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    static constexpr char kFullPerformanceRunSwitch[] = "full-performance-run";
    is_full_performance_run_ =
        command_line->HasSwitch(kFullPerformanceRunSwitch);

    command_line->AppendSwitchASCII(switches::kWindowSize, "1920,1080");

    // Accept the getDisplayMedia() picker for the current tab.
    command_line->AppendSwitch(switches::kThisTabCaptureAutoAccept);

    // Despite the name, this means "don't use a software GL implementation".
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    CHECK(embedded_test_server()->Start());
  }

 protected:
  perf_test::PerfResultReporter CreateReporter() const {
    perf_test::PerfResultReporter reporter("TabCapture.", "direct");
    reporter.RegisterImportantMetric(kMetricCaptureMs, "ms");
    reporter.RegisterImportantMetric(kMetricCaptureFailRatePercent, "percent");
    reporter.RegisterImportantMetric(kMetricCaptureLatencyMs, "ms");
    reporter.RegisterImportantMetric(kMetricRendererFrameDrawMs, "ms");
    return reporter;
  }

  // A full run must produce every result. A quick run may be too short to, so
  // it only logs what is missing.
  void CheckResult(bool reported, std::string_view what) const {
    if (is_full_performance_run_) {
      EXPECT_TRUE(reported) << "Could not report " << what;
    } else {
      LOG_IF(WARNING, !reported)
          << "Could not report " << what << " in a quick run.";
    }
  }

  // Returns the events named `event_name`, trimmed at both ends in a full run,
  // or nullopt if there are too few to trim.
  std::optional<TraceEventVector> GetTrimmedEvents(
      TraceAnalyzer& analyzer,
      std::string_view event_name) const {
    TraceEventVector events = QueryTraceEvents(analyzer, event_name);
    const size_t trim = is_full_performance_run_ ? kTrimEvents : 0;
    if (events.size() < 2 * trim + kMinEventsForQuickRun) {
      LOG(ERROR) << "Too few " << event_name << " events: " << events.size();
      return std::nullopt;
    }
    if (is_full_performance_run_ &&
        events.size() < 2 * trim + kMinEventsForFullRun) {
      LOG(WARNING) << "Fewer " << event_name
                   << " events than expected for a full run: " << events.size();
    }
    return TraceEventVector(events.begin() + trim, events.end() - trim);
  }

  // Reports the mean and standard deviation of the time between events named
  // `event_name`.
  bool ReportRate(TraceAnalyzer& analyzer,
                  perf_test::PerfResultReporter& reporter,
                  std::string_view event_name,
                  std::string_view metric) const {
    const std::optional<TraceEventVector> events =
        GetTrimmedEvents(analyzer, event_name);
    trace_analyzer::RateStats stats;
    if (!events || !trace_analyzer::GetRateStats(*events, &stats, nullptr)) {
      return false;
    }
    reporter.AddResultMeanAndError(
        metric, base::StringPrintf("%f,%f", stats.mean_us / 1000.0,
                                   stats.standard_deviation_us / 1000.0));
    return true;
  }

  // Reports the mean and standard deviation of capture latency, from the begin
  // to the end of each capture event.
  bool ReportLatency(TraceAnalyzer& analyzer,
                     perf_test::PerfResultReporter& reporter) const {
    const std::optional<TraceEventVector> events =
        GetTrimmedEvents(analyzer, kEventCapture);
    if (!events) {
      return false;
    }
    const std::optional<MeanAndStdDev> latency_ms =
        ComputeMeanAndStdDev(GetDurationsMs(*events));
    if (!latency_ms) {
      return false;
    }
    reporter.AddResultMeanAndError(
        kMetricCaptureLatencyMs,
        base::StringPrintf("%f,%f", latency_ms->mean, latency_ms->std_dev));
    return true;
  }

  // Reports the percentage of capture events that did not succeed.
  bool ReportFailRate(TraceAnalyzer& analyzer,
                      perf_test::PerfResultReporter& reporter) const {
    const std::optional<TraceEventVector> events =
        GetTrimmedEvents(analyzer, kEventCapture);
    if (!events) {
      return false;
    }
    const std::optional<double> fail_rate = ComputeFailRatePercent(*events);
    if (!fail_rate) {
      return false;
    }
    reporter.AddResult(kMetricCaptureFailRatePercent, *fail_rate);
    return true;
  }

  // Runs the browser with tracing enabled for the observation period, and
  // returns the collected trace.
  std::unique_ptr<TraceAnalyzer> TraceAndObserve() const {
    static constexpr char kCategories[] = "gpu,gpu.capture";
    static constexpr base::TimeDelta kFullRunObservationPeriod =
        base::Seconds(15);
    static constexpr base::TimeDelta kQuickRunObservationPeriod =
        base::Seconds(4);

    // Keep BEST_EFFORT tasks in the browser process from adding noise while
    // tracing.
    base::ScopedBestEffortExecutionFence best_effort_fence;

    {
      // Wait until all child processes have started tracing.
      base::RunLoop run_loop;
      CHECK(tracing::BeginTracingWithTraceConfig(
          base::trace_event::TraceConfig(
              kCategories, base::trace_event::RECORD_CONTINUOUSLY),
          run_loop.QuitClosure()));
      run_loop.Run();
    }

    {
      base::RunLoop run_loop;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          is_full_performance_run_ ? kFullRunObservationPeriod
                                   : kQuickRunObservationPeriod);
      run_loop.Run();
    }

    std::string json_events;
    CHECK(tracing::EndTracing(&json_events));
    VLOG(2) << "Dump of trace events (trace_events.json.gz.b64):\n"
            << MakeBase64EncodedGZippedString(json_events);

    std::unique_ptr<TraceAnalyzer> analyzer(TraceAnalyzer::Create(json_events));
    analyzer->AssociateAsyncBeginEndEvents();
    return analyzer;
  }

 private:
  bool is_full_performance_run_ = false;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TabCapturePerformanceTest, Performance) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kTestWebPagePath)));
  ASSERT_EQ(
      "success",
      content::EvalJs(browser()->GetTabStripModel()->GetActiveWebContents(),
                      kStartCaptureScript));

  // Check after capture has started, since the GPU process may have fallen
  // back to software by now.
  const gpu::GPUInfo gpu_info =
      content::GpuDataManager::GetInstance()->GetGPUInfo();
  if (!IsUsingHardwareGpu(gpu_info)) {
    GTEST_SKIP() << "This test requires a hardware GPU, got: "
                 << gpu_info.gl_renderer;
  }

  std::unique_ptr<TraceAnalyzer> analyzer = TraceAndObserve();

  const TraceEventVector capture_events =
      QueryTraceEvents(*analyzer, kEventCapture);
  EXPECT_GE(capture_events.size(), kMinEventsForQuickRun)
      << "Too few capture events.";
  EXPECT_TRUE(std::ranges::any_of(capture_events, &EventWasSuccessful))
      << "No capture succeeded.";

  perf_test::PerfResultReporter reporter = CreateReporter();
  // Average time between frames drawn by the captured tab's renderer.
  CheckResult(
      ReportRate(*analyzer, reporter, kEventCommitAndDrawCompositorFrame,
                 kMetricRendererFrameDrawMs),
      "renderer frame rate");
  // Average time between captured frames.
  CheckResult(ReportRate(*analyzer, reporter, kEventCapture, kMetricCaptureMs),
              "capture rate");
  CheckResult(ReportLatency(*analyzer, reporter), "capture latency");
  CheckResult(ReportFailRate(*analyzer, reporter), "capture fail rate");
}
