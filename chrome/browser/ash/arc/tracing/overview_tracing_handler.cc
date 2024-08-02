// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/tracing/overview_tracing_handler.h"

#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/linux_util.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/time_override.h"
#include "base/timer/timer.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/tracing/arc_system_stat_collector.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/ash/arc/tracing/arc_tracing_model.h"
#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace arc {

struct OverviewTracingHandler::ActiveTrace {
  arc::ArcTracingGraphicsModel model;

  base::FilePath save_model_path;

  // Time filter for tracing, since ARC++ window was activated last until
  // tracing is stopped.
  base::TimeTicks time_min;
  base::TimeTicks time_max;

  // Collects system stat runtime.
  arc::ArcSystemStatCollector system_stat_collector;

  // Information about active task, title and icon.
  std::string task_title;
  std::vector<unsigned char> task_icon_png;
  base::Time timestamp;

  // This must be destructed on the UI thread, so make it manually-destructable
  // with std::optional.
  std::optional<base::OneShotTimer> stop_timer;

  arc::PresentFramesTracer present_frames;
};

namespace {

class ProcessFilterPassAll : public base::ProcessFilter {
 public:
  ProcessFilterPassAll() = default;

  ProcessFilterPassAll(const ProcessFilterPassAll&) = delete;
  ProcessFilterPassAll& operator=(const ProcessFilterPassAll&) = delete;

  ~ProcessFilterPassAll() override = default;

  // base::ProcessFilter:
  bool Includes(const base::ProcessEntry& process) const override {
    return true;
  }
};

// Reads name of thread from /proc/pid/task/tid/status.
bool ReadNameFromStatus(pid_t pid, pid_t tid, std::string* out_name) {
  std::string status;
  if (!base::ReadFileToString(base::FilePath(base::StringPrintf(
                                  "/proc/%d/task/%d/status", pid, tid)),
                              &status)) {
    return false;
  }
  base::StringTokenizer tokenizer(status, "\n");
  while (tokenizer.GetNext()) {
    std::string_view value_str(tokenizer.token_piece());
    if (!base::StartsWith(value_str, "Name:")) {
      continue;
    }
    std::vector<std::string_view> split_value_str = base::SplitStringPiece(
        value_str, "\t", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    DCHECK_EQ(2U, split_value_str.size());
    *out_name = std::string(split_value_str[1]);
    return true;
  }

  return false;
}

// Helper that clarifies thread and process names. Tracing events may not have
// enough data for this. Also it determines the process pid the thread belongs
// to.
void UpdateThreads(arc::ArcSystemModel::ThreadMap* threads) {
  ProcessFilterPassAll filter_pass_all;
  base::ProcessIterator process_iterator(&filter_pass_all);

  std::vector<pid_t> tids;
  std::string name;
  for (const auto& process : process_iterator.Snapshot()) {
    tids.clear();
    base::GetThreadsForProcess(process.pid(), &tids);
    bool process_in_use = threads->find(process.pid()) != threads->end();
    for (pid_t tid : tids) {
      if (threads->find(tid) != threads->end()) {
        process_in_use = true;
        (*threads)[tid].pid = process.pid();
        if (!ReadNameFromStatus(process.pid(), tid, &(*threads)[tid].name)) {
          LOG(WARNING) << "Failed to update thread name " << tid;
        }
      }
    }
    if (process_in_use) {
      (*threads)[process.pid()].pid = process.pid();
      if (!ReadNameFromStatus(process.pid(), process.pid(),
                              &(*threads)[process.pid()].name)) {
        LOG(WARNING) << "Failed to update process name " << process.pid();
      }
    }
  }
}

std::unique_ptr<OverviewTracingResult> BuildGraphicsModel(
    const std::string& data,
    std::unique_ptr<OverviewTracingHandler::ActiveTrace> trace,
    const base::FilePath& model_path) {
  DCHECK(trace);

  if (base::FeatureList::IsEnabled(arc::kSaveRawFilesOnTracing)) {
    const base::FilePath raw_path =
        model_path.DirName().Append(model_path.BaseName().value() + "_raw");
    const base::FilePath system_path =
        model_path.DirName().Append(model_path.BaseName().value() + "_system");
    if (!base::WriteFile(base::FilePath(raw_path), data)) {
      LOG(ERROR) << "Failed to save raw trace model to " << raw_path.value();
    }
    const std::string system_raw =
        trace->system_stat_collector.SerializeToJson();
    if (!base::WriteFile(base::FilePath(system_path), system_raw)) {
      LOG(ERROR) << "Failed to save system model to " << system_path.value();
    }
  }

  arc::ArcTracingModel common_model;
  const base::TimeTicks time_min_clamped =
      std::max(trace->time_min,
               trace->time_max - trace->system_stat_collector.max_interval());
  common_model.SetMinMaxTime(
      (time_min_clamped - base::TimeTicks()).InMicroseconds(),
      (trace->time_max - base::TimeTicks()).InMicroseconds());

  if (!common_model.Build(data)) {
    return std::make_unique<OverviewTracingResult>(
        base::Value(), base::FilePath(), "Failed to process tracing data");
  }

  trace->system_stat_collector.Flush(trace->time_min, trace->time_max,
                                     &common_model.system_model());

  trace->model.set_skip_structure_validation();
  if (!trace->model.Build(common_model, trace->present_frames)) {
    return std::make_unique<OverviewTracingResult>(
        base::Value(), base::FilePath(), "Failed to build tracing model");
  }

  UpdateThreads(&trace->model.system_model().thread_map());
  trace->model.set_app_title(trace->task_title);
  trace->model.set_app_icon_png(trace->task_icon_png);
  trace->model.set_platform(base::GetLinuxDistro());
  trace->model.set_timestamp(trace->timestamp);
  base::Value::Dict model = trace->model.Serialize();

  std::string json_content;
  base::JSONWriter::WriteWithOptions(
      model, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_content);
  DCHECK(!json_content.empty());

  if (!base::WriteFile(model_path, json_content)) {
    LOG(ERROR) << "Failed serialize model to " << model_path.value() << ".";
  }

  return std::make_unique<OverviewTracingResult>(
      base::Value(std::move(model)), model_path, "Tracing model is ready");
}

base::trace_event::TraceConfig GetTracingConfig() {
  base::trace_event::TraceConfig config(
      "-*," TRACE_DISABLED_BY_DEFAULT("display.framedisplayed"),
      base::trace_event::RECORD_CONTINUOUSLY);
  config.EnableSystrace();
  config.EnableSystraceEvent("i915:intel_gpu_freq_change");
  config.EnableSystraceEvent("drm_msm_gpu:msm_gpu_freq_change");
  return config;
}

}  // namespace

std::string OverviewTracingHandler::GetModelBaseNameFromTitle(
    std::string_view title,
    base::Time timestamp) {
  constexpr size_t kMaxNameSize = 32;
  char normalized_name[kMaxNameSize];
  size_t index = 0;
  for (char c : title) {
    if (index == kMaxNameSize - 1) {
      break;
    }
    c = base::ToLowerASCII(c);
    if (c == ' ') {
      normalized_name[index++] = '_';
      continue;
    }
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      normalized_name[index++] = c;
    }
  }
  normalized_name[index] = 0;

  const std::string time =
      base::UnlocalizedTimeFormatWithPattern(timestamp, "yyyy-MM-dd_HH-mm-ss");
  return base::StringPrintf("overview_tracing_%s_%s.json", normalized_name,
                            time.c_str());
}

OverviewTracingHandler::OverviewTracingHandler(
    ArcWindowFocusChangeCb arc_window_focus_change)
    : arc_window_focus_change_(arc_window_focus_change),
      wm_helper_(exo::WMHelper::HasInstance() ? exo::WMHelper::GetInstance()
                                              : nullptr) {
  DCHECK(wm_helper_);

  aura::Window* const current_active = wm_helper_->GetActiveWindow();
  if (current_active) {
    OnWindowActivated(ActivationReason::ACTIVATION_CLIENT /* not used */,
                      current_active, nullptr);
  }
  wm_helper_->AddActivationObserver(this);
}

OverviewTracingHandler::~OverviewTracingHandler() {
  wm_helper_->RemoveActivationObserver(this);
  DiscardActiveArcWindow();
}

void OverviewTracingHandler::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  // Handle ARC current active window if any. This stops any ongoing trace.
  DiscardActiveArcWindow();

  if (!gained_active) {
    return;
  }

  if (!arc::GetWindowTaskId(gained_active).has_value()) {
    return;
  }

  arc_active_window_ = gained_active;
  arc_active_window_->AddObserver(this);
  if (arc_window_focus_change_) {
    arc_window_focus_change_.Run(arc_active_window_);
  }

  exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
  CHECK(surface);

  // We observe the _root_ window surface rather than the layer that receives
  // the buffer attachment. This is because it makes locating the surface
  // much easier. We will not have a noticeable delay in monitoring this
  // higher-level window since the wl_surface_commit calls by the client
  // percolate up a single thread's call stack. The root surface's commit is
  // at [0], and the ApplyPending...Changes calls there are committing
  // subsurfaces.
  // [0]
  // https://source.corp.google.com/h/googleplex-android/platform/superproject/base/+/rvc-arc:vendor/google_arc/libs/wayland_service/wayland_layer_container_window.cpp;l=73-79;drc=7bf4887dc1838167c63ae6c0fc514aaab9551e2a
  //
  // We do not need to observe the Attach event since that immediately precedes
  // the commit, and we don't care about the buffer ID.

  // TODO(matvore): can the root surface change after the window is created?
  surface->AddSurfaceObserver(this);
}

void OverviewTracingHandler::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  DCHECK_EQ(arc_active_window_, window);
  if (key != aura::client::kAppIconKey) {
    return;
  }

  if (active_trace_) {
    UpdateActiveArcWindowInfo();
  }
}

void OverviewTracingHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(arc_active_window_, window);
  DiscardActiveArcWindow();
}

void OverviewTracingHandler::OnSurfaceDestroying(exo::Surface* surface) {
  DiscardActiveArcWindow();
}

void OverviewTracingHandler::OnCommit(exo::Surface* surface) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!active_trace_) {
    return;
  }

  active_trace_->present_frames.AddCommit(SystemTicksNow());
  active_trace_->present_frames.ListenForPresent(surface);
}

void OverviewTracingHandler::UpdateActiveArcWindowInfo() {
  DCHECK(arc_active_window_);
  DCHECK(active_trace_);

  active_trace_->task_title =
      base::UTF16ToASCII(arc_active_window_->GetTitle());
  active_trace_->task_icon_png.clear();

  const gfx::ImageSkia* app_icon =
      arc_active_window_->GetProperty(aura::client::kAppIconKey);
  if (app_icon) {
    gfx::PNGCodec::EncodeBGRASkBitmap(
        app_icon->GetRepresentation(1.0f).GetBitmap(),
        false /* discard_transparency */, &active_trace_->task_icon_png);
  }
}

void OverviewTracingHandler::DiscardActiveArcWindow() {
  if (active_trace_) {
    StopTracing();
  }

  if (!arc_active_window_) {
    return;
  }

  exo::Surface* const surface = exo::GetShellRootSurface(arc_active_window_);
  if (surface) {
    surface->RemoveSurfaceObserver(this);
  }

  if (arc_window_focus_change_) {
    arc_window_focus_change_.Run(nullptr);
  }
  arc_active_window_->RemoveObserver(this);
  arc_active_window_ = nullptr;
}

base::Time OverviewTracingHandler::Now() {
  return base::Time::Now();
}

void OverviewTracingHandler::StartTracingOnController(
    const base::trace_event::TraceConfig& trace_config,
    content::TracingController::StartTracingDoneCallback after_start) {
  content::TracingController::GetInstance()->StartTracing(
      trace_config, std::move(after_start));
}

void OverviewTracingHandler::StopTracingOnController(
    content::TracingController::CompletionCallback after_stop) {
  auto* const controller = content::TracingController::GetInstance();

  if (!controller->IsTracing()) {
    LOG(WARNING) << "TracingController has already stopped tracing";
    return;
  }

  controller->StopTracing(
      content::TracingController::CreateStringEndpoint(std::move(after_stop)));
}

OverviewTracingHandler::AppWindowList OverviewTracingHandler::AllAppWindows()
    const {
  auto* app_service = apps::AppServiceProxyFactory::GetForProfile(
      ProfileManager::GetPrimaryUserProfile());
  AppWindowList windows;
  if (app_service) {
    app_service->InstanceRegistry().ForEachInstance(
        [&windows](const auto& update) {
          windows.emplace_back(update.Window());
        });
  }
  return windows;
}

void OverviewTracingHandler::StartTracing(const base::FilePath& save_path,
                                          base::TimeDelta max_time) {
  active_trace_ = std::make_unique<ActiveTrace>();

  active_trace_->save_model_path = save_path;
  active_trace_->system_stat_collector.Start(max_time);
  active_trace_->timestamp = Now();
  UpdateActiveArcWindowInfo();
  active_trace_->time_min = SystemTicksNow();

  StartTracingOnController(
      GetTracingConfig(),
      base::BindOnce(&OverviewTracingHandler::OnTracingStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OverviewTracingHandler::StopTracing() {
  if (start_build_model_) {
    start_build_model_.Run();
  }

  // |stop_timer| will already be nullopt in the case that the window was or
  // deactivated before OnTracingStarted was invoked.
  if (!active_trace_->stop_timer) {
    active_trace_.reset();
    return;
  }
  active_trace_->stop_timer.reset();

  active_trace_->time_max = SystemTicksNow();
  active_trace_->system_stat_collector.Stop();

  StopTracingOnController(
      base::BindOnce(&OverviewTracingHandler::OnTracingStopped,
                     weak_ptr_factory_.GetWeakPtr(), std::move(active_trace_)));
}

OverviewTracingHandler::AppWindowList
OverviewTracingHandler::NonTraceTargetWindows() const {
  auto windows = AllAppWindows();
  auto arc_window_pos =
      std::find(windows.begin(), windows.end(), arc_active_window_);
  if (arc_window_pos != windows.end()) {
    windows.erase(arc_window_pos);
  }

  return windows;
}

bool OverviewTracingHandler::is_tracing() const {
  return active_trace_ != nullptr;
}

bool OverviewTracingHandler::arc_window_is_active() const {
  return arc_active_window_ != nullptr;
}

base::TimeTicks OverviewTracingHandler::SystemTicksNow() {
  return TRACE_TIME_TICKS_NOW();
}

void OverviewTracingHandler::OnTracingStarted() {
  // This is an asynchronous call and it may arrive after tracing is actually
  // stopped.
  if (!active_trace_) {
    return;
  }

  active_trace_->stop_timer.emplace();
  active_trace_->stop_timer->Start(
      FROM_HERE, active_trace_->system_stat_collector.max_interval(),
      base::BindOnce(&OverviewTracingHandler::StopTracing,
                     base::Unretained(this)));
}

void OverviewTracingHandler::OnTracingStopped(
    std::unique_ptr<ActiveTrace> trace,
    std::unique_ptr<std::string> trace_data) {
  std::string string_data;
  string_data.swap(*trace_data);

  const base::FilePath model_path = trace->save_model_path.AppendASCII(
      GetModelBaseNameFromTitle(trace->task_title, trace->timestamp));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BuildGraphicsModel, std::move(string_data),
                     std::move(trace), model_path),
      base::BindOnce(graphics_model_ready_));
}

}  // namespace arc
