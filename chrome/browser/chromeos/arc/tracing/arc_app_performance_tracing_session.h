// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/exo/surface_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace exo {
class Surface;
}  // namespace exo

namespace arc {

class ArcAppPerformanceTracing;
class ArcAppPerformanceTracingCustomSession;

// Implements Surface commit tracing for the target window.
class ArcAppPerformanceTracingSession : public exo::SurfaceObserver {
 public:
  explicit ArcAppPerformanceTracingSession(ArcAppPerformanceTracing* owner);
  ~ArcAppPerformanceTracingSession() override;

  // Performs initial scheduling of tracing based on session type.
  virtual void Schedule() = 0;

  // Casts this session to |ArcAppPerformanceTracingCustomSession|.
  virtual ArcAppPerformanceTracingCustomSession* AsCustomSession();

  // exo::SurfaceObserver:
  void OnSurfaceDestroying(exo::Surface* surface) override;
  void OnCommit(exo::Surface* surface) override;

  // Fires tracing timeout for testing.
  void FireTimerForTesting();
  // Add one more sample for testing.
  void OnCommitForTesting(const base::Time& timestamp);

  bool tracing_active() const { return tracing_active_; }
  ArcAppPerformanceTracing* owner() { return owner_; }
  const ArcAppPerformanceTracing* owner() const { return owner_; }
  const aura::Window* window() const { return window_; }

 protected:
  // Called when tracing is done.
  virtual void OnTracingDone(double fps,
                             double commit_deviation,
                             double render_quality) = 0;
  virtual void OnTracingFailed() = 0;

  // Schedules tracing with a delay and for specific amount of time. If
  // |tracing_period| is 0 then it means manual tracing and |StopAndAnalyze|
  // should be called in order to get results.
  void ScheduleInternal(bool detect_idles,
                        const base::TimeDelta& start_delay,
                        const base::TimeDelta& tracing_period);

  // Stops current tracing and analyzes results.
  void StopAndAnalyzeInternal();

 private:
  // Starts tracing by observing commits to the |exo::Surface| attached to the
  // current |window_|.
  void Start();

  // Stops tracing for the current |window_|.
  void Stop();

  // Handles the next commit update. This is unified handler for testing and
  // production code.
  void HandleCommit(const base::Time& timestamp);

  // Stops current tracing, analyzes captured tracing results and schedules the
  // next tracing for the current |window_|. |tracing_period| indicates the time
  // spent for tracing.
  void Analyze(base::TimeDelta tracing_period);

  // Unowned pointers.
  ArcAppPerformanceTracing* const owner_;
  aura::Window* const window_;

  // Timer to start Surface commit tracing delayed.
  base::OneShotTimer tracing_timer_;

  // Start time of tracing.
  base::TimeTicks tracing_start_;

  // Requested tracing period.
  base::TimeDelta tracing_period_;

  // Set to true in case automatic idle detection is required.
  bool detect_idles_ = false;

  // Timestamp of last commit event.
  base::Time last_commit_timestamp_;

  // Accumulator for commit deltas.
  std::vector<base::TimeDelta> frame_deltas_;

  // Indicates that tracing is in active state.
  bool tracing_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracingSession);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_APP_PERFORMANCE_TRACING_SESSION_H_
