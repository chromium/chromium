// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_
#define CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/tracing/present_frames_tracer.h"
#include "components/exo/surface_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace exo {
class ScopedSurface;
class Surface;
}  // namespace exo

namespace arc {

struct PerfTraceResult {
  double fps, commit_deviation, perceived_fps, present_deviation,
      render_quality, janks_per_minute, janks_percentage;
};

using TicksNowCallback = base::RepeatingCallback<base::TimeTicks()>;

// Implements Surface commit tracing for the target window.
class ArcAppPerformanceTracingSession : public exo::SurfaceObserver {
 public:
  // Called when a trace is complete under one of the following conditions:
  //   a. scheduled timed completion
  //   b. error mid-trace
  //   c. |Finish| method called
  // The optional is empty iff the trace failed.
  using DoneCallback =
      base::OnceCallback<void(const std::optional<PerfTraceResult>&)>;

  ArcAppPerformanceTracingSession(aura::Window* window,
                                  TicksNowCallback ticks_now_callback_);

  ArcAppPerformanceTracingSession(const ArcAppPerformanceTracingSession&) =
      delete;
  ArcAppPerformanceTracingSession& operator=(
      const ArcAppPerformanceTracingSession&) = delete;

  ~ArcAppPerformanceTracingSession() override;

  // exo::SurfaceObserver:
  void OnSurfaceDestroying(exo::Surface* surface) override;
  void OnCommit(exo::Surface* surface) override;

  // Fires tracing timeout for testing.
  void FireTimerForTesting();
  // Returns the delay requested before starting the test the last time Schedule
  // was called.
  base::TimeDelta timer_delay_for_testing() const;

  bool tracing_active() const { return tracing_active_; }
  const aura::Window* window() const { return window_; }

  // Returns whether |present_frames_| is non-empty.
  bool HasPresentFrames() const;

  // Schedules tracing with a delay and for specific amount of time. If
  // |tracing_period| is 0 then it means manual tracing and |Finish|
  // should be called in order to get results.
  void Schedule(bool detect_idles,
                const base::TimeDelta& start_delay,
                const base::TimeDelta& tracing_period,
                DoneCallback on_done);

  // Call to terminate the trace immediately. This will cause the DoneCallback
  // to be called before returning, with either a successful or failed result.
  void Finish();

  void set_trace_real_presents(bool trace) { trace_real_presents_ = trace; }

 private:
  // Starts tracing by observing commits to the |exo::Surface| attached to the
  // current |window_|.
  void Start();

  // Stops tracing for the current |window_|. This cleans up trace fields and
  // invokes on_done_ with the given results, which may be an error.
  void Stop(const std::optional<PerfTraceResult>& result);

  // Stops current tracing, analyzes captured tracing results and schedules the
  // next tracing for the current |window_|. |tracing_period| indicates the time
  // spent for tracing.
  void Analyze(base::TimeDelta tracing_period);

  // Returns true if idle detection is enabled and the last activity exceeds the
  // allowed time between commits. Otherwise, returns false and marks the
  // current time as the last commit.
  bool DetectIdle();

  // Unowned pointers.
  const raw_ptr<aura::Window> window_;

  // Used for automatic observer adding/removing.
  std::unique_ptr<exo::ScopedSurface> scoped_surface_;

  // Timer to start Surface commit tracing delayed.
  base::OneShotTimer tracing_timer_;

  // Start time of tracing.
  base::TimeTicks tracing_start_;

  // Requested tracing period.
  base::TimeDelta tracing_period_;

  // Set to true in case automatic idle detection is required.
  bool detect_idles_ = false;

  // Last time trace was started or a commit occurred, for purposes of idle
  // detection.
  base::TimeTicks last_active_time_;

  // Traces and records frame timing.
  std::optional<PresentFramesTracer> frames_;

  // Times at which frames were recorded.
  std::vector<base::TimeTicks> frame_times_;

  // Indicates that tracing is in active state.
  bool tracing_active_ = false;

  // Whether we use PresentFramesTracer::ListenForPresent or ::AddPresent to
  // record traces.
  bool trace_real_presents_ = true;

  TicksNowCallback ticks_now_callback_;

  DoneCallback on_done_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_
