// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/frame_rate_estimator.h"

#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {
namespace {

constexpr auto kInputPriorityDelay = base::Milliseconds(250);

}  // namespace

FrameRateEstimator::FrameRateEstimator(base::SequencedTaskRunner* task_runner)
    : notifier_(
          task_runner,
          base::BindRepeating(&FrameRateEstimator::OnExitInputPriorityMode,
                              base::Unretained(this)),
          kInputPriorityDelay) {}

FrameRateEstimator::~FrameRateEstimator() = default;

void FrameRateEstimator::SetFrameEstimationEnabled(bool enabled) {
  if (enabled == frame_rate_estimation_enabled_)
    return;

  frame_rate_estimation_enabled_ = enabled;
  last_draw_time_ = base::TimeTicks();
  num_of_consecutive_frames_with_min_delta_ = 0u;
}

void FrameRateEstimator::WillDraw(base::TimeTicks now) {
  if (!frame_rate_estimation_enabled_ || input_priority_mode_)
    return;

  if (last_draw_time_ == base::TimeTicks()) {
    last_draw_time_ = now;
    return;
  }

  auto draw_delta = now - last_draw_time_;
  last_draw_time_ = now;

  // If we see that the page is animating consistently at 30 fps or more, then
  // we assume that BeginFrames can not be throttled. But if the animation
  // frequency is lower than that, then using a lower frame rate is permitted.
  // The delta below is to account for minor offsets in frame times.
  constexpr auto kFudgeDelta = base::Milliseconds(1);
  constexpr auto kMinDelta =
      (viz::BeginFrameArgs::DefaultInterval() * 2) - kFudgeDelta;
  if (draw_delta < kMinDelta)
    num_of_consecutive_frames_with_min_delta_++;
  else
    num_of_consecutive_frames_with_min_delta_ = 0u;
}

base::TimeDelta FrameRateEstimator::GetPreferredInterval() const {
  if (!frame_rate_estimation_enabled_ || input_priority_mode_)
    return viz::BeginFrameArgs::MinInterval();

  constexpr size_t kMinNumOfFramesWithMinDelta = 4u;
  if (num_of_consecutive_frames_with_min_delta_ >= kMinNumOfFramesWithMinDelta)
    return viz::BeginFrameArgs::MinInterval();

  return viz::BeginFrameArgs::DefaultInterval() * 2;
}

void FrameRateEstimator::NotifyInputEvent() {
  if (!frame_rate_estimation_enabled_)
    return;

  input_priority_mode_ = true;
  notifier_.Schedule();
}

void FrameRateEstimator::OnExitInputPriorityMode() {
  input_priority_mode_ = false;
  last_draw_time_ = base::TimeTicks();
  num_of_consecutive_frames_with_min_delta_ = 0u;
}

}  // namespace cc
