// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Target FPS, all reference devices has 60 FPS.
// TODO(khmel), detect this per device.
constexpr uint64_t kTargetFps = 60;

constexpr auto kTargetFrameTime = base::Seconds(1) / kTargetFps;

// Used for detection the idle. App considered in idle state when there is no
// any commit for |kIdleThresholdFrames| frames.
constexpr uint64_t kIdleThresholdFrames = 10;

}  // namespace

ArcAppPerformanceTracingSession::ArcAppPerformanceTracingSession(
    aura::Window* window,
    TicksNowCallback ticks_now_callback)
    : window_(window), ticks_now_callback_(std::move(ticks_now_callback)) {
  DCHECK(window_);
  DCHECK(ticks_now_callback_);
}

ArcAppPerformanceTracingSession::~ArcAppPerformanceTracingSession() {
  // Discard any active tracing if any.
  Stop();
}

void ArcAppPerformanceTracingSession::Schedule(
    bool detect_idles,
    const base::TimeDelta& start_delay,
    const base::TimeDelta& tracing_period,
    DoneCallback on_done) {
  DCHECK(!tracing_active_);
  DCHECK(!tracing_timer_.IsRunning());
  detect_idles_ = detect_idles;
  tracing_period_ = tracing_period;
  on_done_ = std::move(on_done);
  if (start_delay.is_zero()) {
    Start();
    return;
  }
  tracing_timer_.Start(FROM_HERE, start_delay,
                       base::BindOnce(&ArcAppPerformanceTracingSession::Start,
                                      base::Unretained(this)));
}

void ArcAppPerformanceTracingSession::Finish() {
  DCHECK(tracing_active_);
  Analyze(ticks_now_callback_.Run() - tracing_start_);
}

void ArcAppPerformanceTracingSession::OnSurfaceDestroying(
    exo::Surface* surface) {
  // |scoped_surface_| might be already reset in case window is destroyed
  // first.
  DCHECK(!scoped_surface_ || (scoped_surface_->get() == surface));
  Stop();
}

void ArcAppPerformanceTracingSession::FireTimerForTesting() {
  tracing_timer_.FireNow();
}

base::TimeDelta ArcAppPerformanceTracingSession::timer_delay_for_testing()
    const {
  return tracing_timer_.GetCurrentDelay();
}

void ArcAppPerformanceTracingSession::Start() {
  DCHECK(!tracing_timer_.IsRunning());

  VLOG(1) << "Start tracing.";

  frame_deltas_.clear();
  last_commit_timestamp_ = base::TimeTicks();

  exo::Surface* const surface = exo::GetShellRootSurface(window_);
  DCHECK(surface);
  // Use scoped surface observer to be safe on the surface
  // destruction. |exo::GetShellRootSurface| would fail in case
  // the surface gets destroyed before widget.
  scoped_surface_ =
      std::make_unique<exo::ScopedSurface>(surface, this /* observer */);

  // Schedule result analyzing at the end of tracing.
  tracing_start_ = ticks_now_callback_.Run();
  if (!tracing_period_.is_zero()) {
    // |tracing_period_| is passed to be able to correctly compare expectations
    // in unit tests.
    tracing_timer_.Start(
        FROM_HERE, tracing_period_,
        base::BindOnce(&ArcAppPerformanceTracingSession::Analyze,
                       base::Unretained(this), tracing_period_));
  }
  tracing_active_ = true;
}

void ArcAppPerformanceTracingSession::Stop() {
  tracing_active_ = false;
  tracing_timer_.Stop();
  scoped_surface_.reset();
}

void ArcAppPerformanceTracingSession::OnCommit(exo::Surface* surface) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto timestamp = ticks_now_callback_.Run();
  if (last_commit_timestamp_.is_null()) {
    last_commit_timestamp_ = timestamp;
    return;
  }

  const base::TimeDelta frame_delta = timestamp - last_commit_timestamp_;
  last_commit_timestamp_ = timestamp;

  if (detect_idles_) {
    const uint64_t display_frames_passed =
        base::ClampRound<uint64_t>(frame_delta / kTargetFrameTime);
    if (display_frames_passed >= kIdleThresholdFrames) {
      // Idle is detected, try the next time.
      Stop();
      std::move(on_done_).Run(std::nullopt);
      return;
    }
  }

  frame_deltas_.emplace_back(frame_delta);
}

void ArcAppPerformanceTracingSession::Analyze(base::TimeDelta tracing_period) {
  // No more data is needed, stop active tracing.
  Stop();

  if (frame_deltas_.empty() || tracing_period <= base::TimeDelta()) {
    std::move(on_done_).Run(std::nullopt);
    return;
  }

  // Check last commit timestamp if we are in idle at this moment.
  if (detect_idles_) {
    const base::TimeDelta last_frame_delta =
        ticks_now_callback_.Run() - last_commit_timestamp_;
    if (last_frame_delta >= kTargetFrameTime * kIdleThresholdFrames) {
      // Current idle state is detected, try next time.
      std::move(on_done_).Run(std::nullopt);
      return;
    }
  }

  VLOG(1) << "Analyze tracing.";

  double vsync_error_deviation_accumulator = 0;
  for (const auto& frame_delta : frame_deltas_) {
    // Calculate the number of display frames passed between two updates.
    // Ideally we should have one frame for target FPS. In case the app drops
    // frames, the number of dropped frames would be accounted. The result is
    // fractional part of target frame interval |kTargetFrameTime| and is less
    // or equal half of it.
    const uint64_t display_frames_passed =
        base::ClampRound<uint64_t>(frame_delta / kTargetFrameTime);
    // Calculate difference from the ideal commit time, that should happen with
    // equal delay for each display frame.
    const base::TimeDelta vsync_error =
        frame_delta - display_frames_passed * kTargetFrameTime;
    vsync_error_deviation_accumulator +=
        (vsync_error.InMicrosecondsF() * vsync_error.InMicrosecondsF());
  }
  const double commit_deviation =
      sqrt(vsync_error_deviation_accumulator / frame_deltas_.size());

  std::sort(frame_deltas_.begin(), frame_deltas_.end());
  // Get 10% and 90% indices.
  const size_t lower_position = frame_deltas_.size() / 10;
  const size_t upper_position = frame_deltas_.size() - 1 - lower_position;
  const double render_quality =
      frame_deltas_[lower_position] / frame_deltas_[upper_position];

  const double fps = frame_deltas_.size() / tracing_period.InSecondsF();

  std::move(on_done_).Run(
      PerfTraceResult{fps, commit_deviation, render_quality});
}

}  // namespace arc
