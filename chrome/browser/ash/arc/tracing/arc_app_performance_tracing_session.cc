// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/arc/tracing/arc_graphics_jank_detector.h"
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

double CalcVSyncError(const base::TimeDelta& frame_delta) {
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
  return (vsync_error.InMicrosecondsF() * vsync_error.InMicrosecondsF());
}

int CalcJankCount(const std::deque<int64_t>& presents) {
  int jank_count = 0;
  ArcGraphicsJankDetector jank_detector(base::BindRepeating(
      [](int* out_count, base::Time timestamp) { (*out_count)++; },
      &jank_count));

  // Feed minimum samples into detector to obtain sampling rate.
  for (const auto& ts_usec : presents) {
    jank_detector.OnSample(
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(ts_usec)));
    if (jank_detector.stage() == ArcGraphicsJankDetector::Stage::kActive) {
      break;
    }
  }
  if (jank_detector.stage() != ArcGraphicsJankDetector::Stage::kActive) {
    LOG(ERROR) << "Jank detector was not able to determine rate";
    return 0;
  }

  // Detected rate, now we can feed all presents to detector to find janks.
  jank_detector.SetPeriodFixed(jank_detector.period());
  for (const auto& ts_usec : presents) {
    jank_detector.OnSample(
        base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(ts_usec)));
  }
  return jank_count;
}

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
  Stop(std::nullopt);
}

void ArcAppPerformanceTracingSession::Schedule(
    bool detect_idles,
    const base::TimeDelta& start_delay,
    const base::TimeDelta& tracing_period,
    DoneCallback on_done) {
  DCHECK(!tracing_active());
  DCHECK(!HasPresentFrames());
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
  DCHECK(tracing_active());
  DCHECK(HasPresentFrames());
  Analyze(ticks_now_callback_.Run() - tracing_start_);
}

void ArcAppPerformanceTracingSession::OnSurfaceDestroying(
    exo::Surface* surface) {
  // |scoped_surface_| might be already reset in case window is destroyed
  // first.
  DCHECK(!scoped_surface_ || (scoped_surface_->get() == surface));
  Stop(std::nullopt);
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

  frame_times_.clear();
  frames_.emplace();

  exo::Surface* const surface = exo::GetShellRootSurface(window_);
  DCHECK(surface);
  // Use scoped surface observer to be safe on the surface
  // destruction. |exo::GetShellRootSurface| would fail in case
  // the surface gets destroyed before widget.
  scoped_surface_ =
      std::make_unique<exo::ScopedSurface>(surface, this /* observer */);

  // Schedule result analyzing at the end of tracing.
  tracing_start_ = last_active_time_ = ticks_now_callback_.Run();
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

bool ArcAppPerformanceTracingSession::HasPresentFrames() const {
  return frames_.has_value();
}

void ArcAppPerformanceTracingSession::Stop(
    const std::optional<PerfTraceResult>& result) {
  VLOG(1) << "Stop tracing.";
  tracing_active_ = false;
  frames_.reset();
  tracing_timer_.Stop();
  scoped_surface_.reset();
  if (on_done_) {
    std::move(on_done_).Run(result);
  }
}

bool ArcAppPerformanceTracingSession::DetectIdle() {
  if (!detect_idles_) {
    return false;
  }

  const auto now = ticks_now_callback_.Run();
  const auto delta = now - last_active_time_;

  const uint64_t display_frames_passed =
      static_cast<uint64_t>(delta / kTargetFrameTime);

  last_active_time_ = now;
  return display_frames_passed >= kIdleThresholdFrames;
}

void ArcAppPerformanceTracingSession::OnCommit(exo::Surface* surface) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (DetectIdle()) {
    Stop(std::nullopt);
    return;
  }

  frame_times_.emplace_back(ticks_now_callback_.Run());
  if (trace_real_presents_) {
    frames_->ListenForPresent(surface);
  } else {
    frames_->AddPresent(ticks_now_callback_.Run());
  }
}

void ArcAppPerformanceTracingSession::Analyze(base::TimeDelta tracing_period) {
  const auto& presents = frames_->presents();
  const size_t num_presents = presents.size(),
               num_frame_times = frame_times_.size();

  if (num_frame_times < 2 || tracing_period <= base::TimeDelta() ||
      DetectIdle()) {
    LOG(ERROR) << "Failed to meet minimum requirements to analyze tracing";
    Stop(std::nullopt);
    return;
  }

  VLOG(1) << "Analyze tracing.";

  std::vector<base::TimeDelta> commit_deltas, present_deltas;
  PerfTraceResult result;
  commit_deltas.reserve(num_frame_times - 1);
  double vsync_error_deviation_accumulator = 0;
  for (auto fitr = frame_times_.begin() + 1; fitr != frame_times_.end();
       fitr++) {
    const auto frame_delta = *fitr - *(fitr - 1);
    commit_deltas.push_back(frame_delta);
    vsync_error_deviation_accumulator += CalcVSyncError(frame_delta);
  }
  result.commit_deviation =
      sqrt(vsync_error_deviation_accumulator / commit_deltas.size());

  // Number of presents could be zero if display-less device (e.g. Chromebox),
  // in this case skip calculating present metrics with less than two frames.
  result.present_deviation = result.perceived_fps = result.janks_per_minute =
      result.janks_percentage = 0;
  if (num_presents > 1) {
    present_deltas.reserve(num_presents - 1);
    vsync_error_deviation_accumulator = 0;
    for (auto fitr = presents.begin() + 1; fitr != presents.end(); fitr++) {
      const auto frame_delta = base::Microseconds(*fitr - *(fitr - 1));
      present_deltas.push_back(frame_delta);
      vsync_error_deviation_accumulator += CalcVSyncError(frame_delta);
    }
    result.present_deviation =
        sqrt(vsync_error_deviation_accumulator / present_deltas.size());
    result.perceived_fps = num_presents / tracing_period.InSecondsF();
    if (ArcGraphicsJankDetector::IsEnoughSamplesToDetect(num_presents)) {
      const double jank_count = static_cast<double>(CalcJankCount(presents));
      result.janks_per_minute =
          jank_count / (tracing_period.InSecondsF() / 60.0);
      result.janks_percentage = jank_count / num_presents * 100.0;
    }
  }

  std::sort(commit_deltas.begin(), commit_deltas.end());
  // Get 10% and 90% indices.
  const size_t lower_position = commit_deltas.size() / 10;
  const size_t upper_position = commit_deltas.size() - 1 - lower_position;
  result.render_quality =
      commit_deltas[lower_position] / commit_deltas[upper_position];
  result.fps = commit_deltas.size() / tracing_period.InSecondsF();

  Stop(result);
}

}  // namespace arc
