// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_OVERVIEW_TRACING_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_OVERVIEW_TRACING_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/exo/surface_observer.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace exo {
class WMHelper;
}  // namespace exo

namespace aura {
class Window;
}  // namespace aura

namespace arc {

struct OverviewTracingResult {
  // In case model cannot be built/load empty `base::Value` is returned.
  base::Value model;

  // File in which the model JSON is saved.
  base::FilePath path;

  // Error or success message.
  std::string status;
};

class OverviewTracingHandler : public wm::ActivationChangeObserver,
                               public aura::WindowObserver,
                               public exo::SurfaceObserver {
 public:
  struct ActiveTrace;
  using Result = OverviewTracingResult;

  // Called when graphics model is built or load. Extra string parameter
  // contains a status. In case model cannot be built/load empty
  // |base::Value| is returned.
  using GraphicsModelReadyCb =
      base::RepeatingCallback<void(std::unique_ptr<OverviewTracingResult>)>;

  using ArcWindowFocusChangeCb = base::RepeatingCallback<void(aura::Window*)>;

  using StartModelBuildCb = base::RepeatingCallback<void()>;

  std::string GetModelBaseNameFromTitle(std::string_view title,
                                        base::Time timestamp);

  explicit OverviewTracingHandler(
      ArcWindowFocusChangeCb arc_window_focus_change);

  OverviewTracingHandler(const OverviewTracingHandler&) = delete;
  OverviewTracingHandler& operator=(const OverviewTracingHandler&) = delete;

  ~OverviewTracingHandler() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // exo::SurfaceObserver:
  void OnSurfaceDestroying(exo::Surface* surface) override;
  void OnCommit(exo::Surface* surface) override;

  void set_graphics_model_ready_cb(GraphicsModelReadyCb callback) {
    graphics_model_ready_ = std::move(callback);
  }

  void set_start_build_model_cb(StartModelBuildCb callback) {
    start_build_model_ = std::move(callback);
  }

  void StartTracing(const base::FilePath& save_path, base::TimeDelta max_time);
  void StopTracing();

  using AppWindowList = std::vector<raw_ptr<aura::Window>>;

  // Returns the windows which are not the currently-active ARC window, if any.
  AppWindowList NonTraceTargetWindows() const;

  bool is_tracing() const;
  bool arc_window_is_active() const;

 protected:
  aura::Window* arc_active_window_for_testing() const {
    return arc_active_window_;
  }

 private:
  GraphicsModelReadyCb graphics_model_ready_;
  ArcWindowFocusChangeCb arc_window_focus_change_;
  StartModelBuildCb start_build_model_;

  virtual void StartTracingOnController(
      const base::trace_event::TraceConfig& trace_config,
      content::TracingController::StartTracingDoneCallback after_start);
  virtual void StopTracingOnController(
      content::TracingController::CompletionCallback after_stop);

  // Returns all aura windows that we know about, if any. Virtual for testing
  // purposes. Necessary because there are times we won't allow a trace to
  // continue if extraneous apps are open.
  virtual AppWindowList AllAppWindows() const;

  // There is a ScopedTimeClockOverrides for tests that makes this seem
  // redundant, but it is rather awkward to have a single test base which
  // utilizes either system time or mock time, as this must be specified in
  // the constructor, and the childmost test class constructor must be
  // parameterless.
  virtual base::Time Now();

  // Exposed for testing. This implementation uses TRACE_TIME_TICKS_NOW.
  // Returns the timestamp using clock_gettime(CLOCK_MONOTONIC), which is
  // needed for comparison with trace timestamps.
  virtual base::TimeTicks SystemTicksNow();

  void OnTracingStarted();
  void OnTracingStopped(std::unique_ptr<ActiveTrace> trace,
                        std::unique_ptr<std::string> trace_data);

  // Updates title and icon for the active ARC window.
  void UpdateActiveArcWindowInfo();

  // Stops tracking ARC window for janks.
  void DiscardActiveArcWindow();

  std::unique_ptr<ActiveTrace> active_trace_;

  const raw_ptr<exo::WMHelper> wm_helper_;

  raw_ptr<aura::Window> arc_active_window_ = nullptr;

  base::WeakPtrFactory<OverviewTracingHandler> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_OVERVIEW_TRACING_HANDLER_H_
