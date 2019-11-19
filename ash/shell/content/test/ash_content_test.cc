// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/test/ash_content_test.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shelf_config.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "ash/shell/content/embedded_browser.h"
#include "ash/shell/window_type_launcher.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/browser_test_utils.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "testing/perf/luci_test_result.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kTraceDir[] = "trace-dir";
constexpr char kPerfTestPrintUmaMeans[] = "perf-test-print-uma-means";

float GetHistogramMean(const std::string& name) {
  auto* histogram = base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return 0;
  // Use SnapshotFinalDelta() so that it won't contain the samples before the
  // subclass invokes SnapshotDelta() during the test.
  auto samples = histogram->SnapshotFinalDelta();
  DCHECK_NE(0, samples->TotalCount());
  return static_cast<float>(samples->sum()) / samples->TotalCount();
}

perf_test::LuciTestResult CreateTestResult(
    const base::FilePath& trace_file,
    const std::vector<std::string>& tbm_metrics) {
  perf_test::LuciTestResult result =
      perf_test::LuciTestResult::CreateForGTest();
  result.AddOutputArtifactFile("trace/1.json", trace_file, "application/json");
  for (auto& metric : tbm_metrics)
    result.AddTag("tbmv2", metric);

  return result;
}

}  // namespace

class AshContentTest::Tracer {
 public:
  Tracer(base::FilePath trace_dir,
         std::string tracing_categories,
         std::vector<std::string> histograms,
         std::vector<std::string> tbm_metrics)
      : trace_dir_(std::move(trace_dir)),
        tracing_categories_(std::move(tracing_categories)),
        tbm_metrics_(std::move(tbm_metrics)) {
    auto* controller = content::TracingController::GetInstance();
    base::trace_event::TraceConfig config(
        tracing_categories_, base::trace_event::RECORD_CONTINUOUSLY);

    for (const auto& histogram : histograms)
      config.EnableHistogram(histogram);

    base::RunLoop runloop;
    bool result = controller->StartTracing(config, runloop.QuitClosure());
    runloop.Run();
    CHECK(result);
  }

  ~Tracer() {
    base::ScopedAllowBlockingForTesting allow_io;
    CreateTmp();

    {
      base::RunLoop runloop;
      auto trace_data_endpoint = content::TracingController::CreateFileEndpoint(
          trace_file_, runloop.QuitClosure());
      auto* controller = content::TracingController::GetInstance();
      bool result = controller->StopTracing(trace_data_endpoint);
      runloop.Run();
      CHECK(result);
    }

    base::FilePath report_file =
        trace_file_.AddExtension(FILE_PATH_LITERAL("test_result.json"));
    CreateTestResult(trace_file_, tbm_metrics_).WriteToFile(report_file);
  }

  void CreateTmp() {
    CHECK(base::CreateTemporaryFileInDir(trace_dir_, &trace_file_));
  }

  base::FilePath trace_dir_;
  base::FilePath trace_file_;
  std::string tracing_categories_;
  std::vector<std::string> tbm_metrics_;
};

AshContentTest::AshContentTest()
    : enable_trace_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(kTraceDir)) {
  auto* cmd = base::CommandLine::ForCurrentProcess();
  if (enable_trace_) {
    cmd->AppendSwitch(switches::kUseGpuInTests);
    cmd->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
}

AshContentTest::~AshContentTest() = default;

void AshContentTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Add command line arguments that are used by all AshContentTests.
  if (!command_line->HasSwitch(switches::kHostWindowBounds) &&
      !base::SysInfo::IsRunningOnChromeOS()) {
    // Adjusting window location & size so that the ash desktop window fits
    // inside the Xvfb's default resolution. Only do that when not running
    // on device. Otherwise, device display is not properly configured.
    command_line->AppendSwitchASCII(switches::kHostWindowBounds,
                                    "0+0-1280x800");
  }
  content::ContentBrowserTest::SetUp();
}

void AshContentTest::SetUpOnMainThread() {
  content::ContentBrowserTest::SetUpOnMainThread();
  if (enable_trace_) {
    base::FilePath dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(kTraceDir);
    tracer_ = std::make_unique<Tracer>(
        std::move(dir),
        std::move(
            "benchmark,cc,viz,input,latency,gpu,rail,toplevel,ui,views,viz"),
        GetUMAHistogramNames(), GetTimelineBasedMetrics());
  }
  gfx::Size display_size = ash::Shell::GetPrimaryRootWindow()->bounds().size();
  test_window_size_.set_height(
      (display_size.height() - ash::ShelfConfig::Get()->shelf_size()) * 0.95f);
  test_window_size_.set_width(display_size.width() * 0.7f);
}

void AshContentTest::TearDownOnMainThread() {
  tracer_.reset();
  auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool print = command_line->HasSwitch(kPerfTestPrintUmaMeans);
  LOG_IF(INFO, print) << "=== Histogram Means ===";
  for (auto name : GetUMAHistogramNames()) {
    EXPECT_TRUE(!!base::StatisticsRecorder::FindHistogram(name))
        << " missing histogram:" << name;
    LOG_IF(INFO, print) << name << ": " << GetHistogramMean(name);
  }
  LOG_IF(INFO, print) << "=== End Histogram Means ===";

  content::ContentBrowserTest::TearDownOnMainThread();
}

aura::Window* AshContentTest::CreateBrowserWindow(const GURL& url) {
  return ash::shell::EmbeddedBrowser::Create(
      ash::shell::ShellBrowserMainParts::GetBrowserContext(), url,
      gfx::Rect(test_window_size_));
}

aura::Window* AshContentTest::CreateTestWindow() {
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      new ash::shell::WindowTypeLauncher(base::NullCallback(),
                                         base::NullCallback()),
      ash::Shell::GetPrimaryRootWindow(), gfx::Rect(test_window_size_));
  widget->GetNativeView()->SetName("WindowTypeLauncher");
  widget->Show();

  return widget->GetNativeWindow();
}

void AshContentTest::PreRunTestOnMainThread() {
  // We're not calling ContentBrowserTest's method because it performs content
  // shell initialization. Same in PostRunTestOnMainThread.

  // Pump startup related events.
  base::RunLoop().RunUntilIdle();
}

void AshContentTest::PostRunTestOnMainThread() {
  // Cleanup all application windows.
  aura::WindowTracker tracker(
      ash::Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(
          ash::kAllDesks));
  while (!tracker.windows().empty()) {
    auto* widget = views::Widget::GetWidgetForNativeWindow(tracker.Pop());
    widget->Close();
  }

  // Sometimes tests leave Quit tasks in the MessageLoop (for shame), so let's
  // run all pending messages here to avoid preempting the QuitBrowsers tasks.
  // TODO(https://crbug.com/922118): Remove this once it is no longer possible
  // to post QuitCurrent* tasks.
  base::RunLoop().RunUntilIdle();
}

std::vector<std::string> AshContentTest::GetUMAHistogramNames() const {
  return {};
}

std::vector<std::string> AshContentTest::GetTimelineBasedMetrics() const {
  return {"renderingMetric", "umaMetric"};
}
