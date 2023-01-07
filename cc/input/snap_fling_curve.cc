// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/snap_fling_curve.h"

#include <algorithm>
#include <cmath>
#include "build/build_config.h"

namespace cc {
namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr double kDistanceEstimatorScalar = 40;
// The delta to be scrolled in next frame is 0.9 of the delta in last frame.
constexpr double kRatio = 0.9;
#else
constexpr double kDistanceEstimatorScalar = 25;
// The delta to be scrolled in next frame is 0.92 of the delta in last frame.
constexpr double kRatio = 0.92;
#endif
constexpr auto kFrameTime = base::Milliseconds(16);
constexpr base::TimeDelta kMaximumSnapDuration = base::Seconds(5);

double GetDistanceFromDisplacement(gfx::Vector2dF displacement) {
  return std::hypot(displacement.x(), displacement.y());
}

double EstimateFramesFromDistance(double distance) {
  // We approximate scroll deltas as a geometric sequence with the ratio kRatio,
  // and the last scrolled delta should be less or equal than 1, yielding the
  // total distance as (1 - kRatio^(-n)) / (1 - (1 / kRatio)). Solving this
  // could get n as below, which is the total number of deltas in the sequence,
  // and is also the total frames needed to finish the curve.
  return std::ceil(-std::log(1 - distance * (1 - 1 / kRatio)) /
                   std::log(kRatio));
}

double CalculateFirstDelta(double distance, double frames) {
  // distance = first_delta (1 - kRatio^(frames) / (1 - kRatio)).
  // We can get the |first_delta| by solving the equation above.
  return distance * (1 - kRatio) / (1 - std::pow(kRatio, frames));
}

bool IsWithinOnePixel(gfx::Vector2dF actual, gfx::Vector2dF target) {
  return std::abs(actual.x() - target.x()) < 1 &&
         std::abs(actual.y() - target.y()) < 1;
}

}  // namespace

gfx::Vector2dF SnapFlingCurve::EstimateDisplacement(
    const gfx::Vector2dF& first_delta) {
  gfx::Vector2dF destination = first_delta;
  destination.Scale(kDistanceEstimatorScalar);
  return destination;
}

SnapFlingCurve::SnapFlingCurve(const gfx::PointF& start_offset,
                               const gfx::PointF& target_offset,
                               base::TimeTicks first_gsu_time)
    : start_offset_(start_offset),
      total_displacement_(target_offset - start_offset),
      total_distance_(GetDistanceFromDisplacement(total_displacement_)),
      start_time_(first_gsu_time),
      total_frames_(EstimateFramesFromDistance(total_distance_)),
      first_delta_(CalculateFirstDelta(total_distance_, total_frames_)),
      duration_(total_frames_ * kFrameTime),
      is_finished_(total_distance_ == 0) {
  if (is_finished_)
    return;
  ratio_x_ = total_displacement_.x() / total_distance_;
  ratio_y_ = total_displacement_.y() / total_distance_;
}

SnapFlingCurve::~SnapFlingCurve() = default;

double SnapFlingCurve::GetCurrentCurveDistance(base::TimeDelta current_time) {
  const double current_frame = current_time / kFrameTime + 1;
  const double sum =
      first_delta_ * (1 - std::pow(kRatio, current_frame)) / (1 - kRatio);
  return std::min(sum, total_distance_);
}

gfx::Vector2dF SnapFlingCurve::GetScrollDelta(base::TimeTicks time_stamp) {
  if (is_finished_)
    return gfx::Vector2dF();

  // The the snap offset may never be reached due to clamping or other factors.
  // To avoid a never ending snap curve, we force the curve to end if the time
  // has passed |duration_| or the remaining displacement is less than 1.
  base::TimeDelta current_time = time_stamp - start_time_;
  if (current_time >= std::min(duration_, kMaximumSnapDuration) ||
      IsWithinOnePixel(current_displacement_, total_displacement_)) {
    is_finished_ = true;
    return total_displacement_ - current_displacement_;
  }

  double new_distance = GetCurrentCurveDistance(current_time);
  gfx::Vector2dF new_displacement(new_distance * ratio_x_,
                                  new_distance * ratio_y_);

  return new_displacement - current_displacement_;
}

void SnapFlingCurve::UpdateCurrentOffset(const gfx::PointF& current_offset) {
  current_displacement_ = current_offset - start_offset_;
}

bool SnapFlingCurve::IsFinished() const {
  return is_finished_;
}

}  // namespace cc
