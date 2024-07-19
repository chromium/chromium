// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/animation/tween.h"

const double kConstantDuration = 9.0;
const double kDurationDivisor = 60.0;

// 0.7 seconds limit for long-distance programmatic scrolls
const double kDeltaBasedMaxDuration = 0.7 * kDurationDivisor;

const double kInverseDeltaRampStartPx = 120.0;
const double kInverseDeltaRampEndPx = 480.0;
const double kInverseDeltaMinDuration = 6.0;
const double kInverseDeltaMaxDuration = 12.0;

const double kInverseDeltaSlope =
    (kInverseDeltaMinDuration - kInverseDeltaMaxDuration) /
    (kInverseDeltaRampEndPx - kInverseDeltaRampStartPx);

const double kInverseDeltaOffset =
    kInverseDeltaMaxDuration - kInverseDeltaRampStartPx * kInverseDeltaSlope;

using gfx::CubicBezierTimingFunction;
using gfx::LinearTimingFunction;
using gfx::TimingFunction;

namespace cc {

namespace {

constexpr double kImpulseCurveX1 = 0.25;
constexpr double kImpulseCurveX2 = 0.0;
constexpr double kImpulseCurveY2 = 1.0;

constexpr double kImpulseMinDurationMs = 200.0;
constexpr double kImpulseMaxDurationMs = 500.0;
constexpr double kImpulseMillisecondsPerPixel = 1.5;

const double kEpsilon = 0.01f;

static float MaximumDimension(const gfx::Vector2dF& delta) {
  return std::abs(delta.x()) > std::abs(delta.y()) ? delta.x() : delta.y();
}

static std::unique_ptr<TimingFunction> EaseInOutWithInitialSlope(double slope) {
  // Clamp slope to a sane value.
  slope = std::clamp(slope, -1000.0, 1000.0);

  // Based on CubicBezierTimingFunction::EaseType::EASE_IN_OUT preset
  // with first control point scaled.
  const double x1 = 0.42;
  const double y1 = slope * x1;
  return CubicBezierTimingFunction::Create(x1, y1, 0.58, 1);
}

std::unique_ptr<TimingFunction> ImpulseCurveWithInitialSlope(double slope) {
  DCHECK_GE(slope, 0);

  double x1 = kImpulseCurveX1;
  double y1 = 1.0;
  if (x1 * slope < 1.0) {
    y1 = x1 * slope;
  } else {
    x1 = y1 / slope;
  }

  const double x2 = kImpulseCurveX2;
  const double y2 = kImpulseCurveY2;
  return CubicBezierTimingFunction::Create(x1, y1, x2, y2);
}

bool IsNewTargetInOppositeDirection(const gfx::PointF& current_position,
                                    const gfx::PointF& old_target,
                                    const gfx::PointF& new_target) {
  gfx::Vector2dF old_delta = old_target - current_position;
  gfx::Vector2dF new_delta = new_target - current_position;

  // We only declare the new target to be in the "opposite" direction when
  // one of the dimensions doesn't change at all. This may sound a bit strange,
  // but it avoids lots of issues.
  // For instance, if we are moving to the down & right and we are updated to
  // move down & left, then are we moving in the opposite direction? If we don't
  // do the check this way, then it would be considered in the opposite
  // direction and the velocity gets set to 0. The update would therefore look
  // pretty janky.
  if (std::abs(old_delta.x() - new_delta.x()) < kEpsilon) {
    return (old_delta.y() >= 0.0f) != (new_delta.y() >= 0.0f);
  } else if (std::abs(old_delta.y() - new_delta.y()) < kEpsilon) {
    return (old_delta.x() >= 0.0f) != (new_delta.x() >= 0.0f);
  } else {
    return false;
  }
}

base::TimeDelta VelocityBasedDurationBound(gfx::Vector2dF old_delta,
                                           double velocity,
                                           gfx::Vector2dF new_delta) {
  double new_delta_max_dimension = MaximumDimension(new_delta);

  // If we are already at the target, stop animating.
  if (std::abs(new_delta_max_dimension) < kEpsilon)
    return base::TimeDelta();

  // Guard against division by zero.
  if (std::abs(velocity) < kEpsilon) {
    return base::TimeDelta::Max();
  }

  // Estimate how long it will take to reach the new target at our present
  // velocity, with some fudge factor to account for the "ease out".
  double bound = (new_delta_max_dimension / velocity) * 2.5f;

  // If bound < 0 we are moving in the opposite direction.
  return bound < 0 ? base::TimeDelta::Max() : base::Seconds(bound);
}

}  // namespace

std::optional<double>
    ScrollOffsetAnimationCurve::animation_duration_for_testing_;

ScrollOffsetAnimationCurve::ScrollOffsetAnimationCurve(
    const gfx::PointF& target_value,
    AnimationType animation_type,
    std::optional<DurationBehavior> duration_behavior)
    : target_value_(target_value),
      animation_type_(animation_type),
      duration_behavior_(duration_behavior),
      has_set_initial_value_(false) {
  DCHECK_EQ(animation_type == AnimationType::kEaseInOut,
            duration_behavior.has_value());
  switch (animation_type) {
    case AnimationType::kEaseInOut:
      timing_function_ = CubicBezierTimingFunction::CreatePreset(
          CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
      break;
    case AnimationType::kLinear:
      timing_function_ = LinearTimingFunction::Create();
      break;
    case AnimationType::kImpulse:
      timing_function_ = ImpulseCurveWithInitialSlope(0);
      break;
  }
}

ScrollOffsetAnimationCurve::ScrollOffsetAnimationCurve(
    const gfx::PointF& target_value,
    std::unique_ptr<TimingFunction> timing_function,
    AnimationType animation_type,
    std::optional<DurationBehavior> duration_behavior)
    : target_value_(target_value),
      timing_function_(std::move(timing_function)),
      animation_type_(animation_type),
      duration_behavior_(duration_behavior),
      has_set_initial_value_(false) {
  DCHECK_EQ(animation_type == AnimationType::kEaseInOut,
            duration_behavior.has_value());
}

ScrollOffsetAnimationCurve::~ScrollOffsetAnimationCurve() = default;

// static
base::TimeDelta ScrollOffsetAnimationCurve::EaseInOutSegmentDuration(
    const gfx::Vector2dF& delta,
    DurationBehavior duration_behavior,
    base::TimeDelta delayed_by) {
  double duration = kConstantDuration;
  if (!animation_duration_for_testing_) {
    switch (duration_behavior) {
      case DurationBehavior::kConstant:
        duration = kConstantDuration;
        break;
      case DurationBehavior::kDeltaBased:
        duration =
            std::min<double>(std::sqrt(std::abs(MaximumDimension(delta))),
                             kDeltaBasedMaxDuration);
        break;
      case DurationBehavior::kInverseDelta:
        duration = kInverseDeltaOffset +
                   std::abs(MaximumDimension(delta)) * kInverseDeltaSlope;
        duration = std::clamp(duration, kInverseDeltaMinDuration,
                              kInverseDeltaMaxDuration);
        break;
    }
    duration /= kDurationDivisor;
  } else {
    duration = animation_duration_for_testing_.value();
  }

  base::TimeDelta delay_adjusted_duration =
      base::Seconds(duration) - delayed_by;
  return (delay_adjusted_duration >= base::TimeDelta())
             ? delay_adjusted_duration
             : base::TimeDelta();
}

base::TimeDelta ScrollOffsetAnimationCurve::EaseInOutBoundedSegmentDuration(
    const gfx::Vector2dF& new_delta,
    base::TimeDelta t,
    base::TimeDelta delayed_by) {
  gfx::Vector2dF old_delta = target_value_ - initial_value_;
  double velocity = CalculateVelocity(t);

  // Use the velocity-based duration bound when it is less than the constant
  // segment duration. This minimizes the "rubber-band" bouncing effect when
  // |velocity| is large and |new_delta| is small.
  return std::min(EaseInOutSegmentDuration(
                      new_delta, duration_behavior_.value(), delayed_by),
                  VelocityBasedDurationBound(old_delta, velocity, new_delta));
}

base::TimeDelta ScrollOffsetAnimationCurve::SegmentDuration(
    const gfx::Vector2dF& delta,
    base::TimeDelta delayed_by,
    std::optional<double> velocity) {
  switch (animation_type_) {
    case AnimationType::kEaseInOut:
      DCHECK(duration_behavior_.has_value());
      return EaseInOutSegmentDuration(delta, duration_behavior_.value(),
                                      delayed_by);
    case AnimationType::kLinear:
      DCHECK(velocity.has_value());
      return LinearSegmentDuration(delta, delayed_by, velocity.value());
    case AnimationType::kImpulse:
      return ImpulseSegmentDuration(delta, delayed_by);
  }
}

// static
base::TimeDelta ScrollOffsetAnimationCurve::LinearSegmentDuration(
    const gfx::Vector2dF& delta,
    base::TimeDelta delayed_by,
    float velocity) {
  double duration_in_seconds =
      (animation_duration_for_testing_.has_value())
          ? animation_duration_for_testing_.value()
          : std::abs(MaximumDimension(delta) / velocity);
  base::TimeDelta delay_adjusted_duration =
      base::Seconds(duration_in_seconds) - delayed_by;
  return (delay_adjusted_duration >= base::TimeDelta())
             ? delay_adjusted_duration
             : base::TimeDelta();
}

// static
base::TimeDelta ScrollOffsetAnimationCurve::ImpulseSegmentDuration(
    const gfx::Vector2dF& delta,
    base::TimeDelta delayed_by) {
  base::TimeDelta duration;
  if (animation_duration_for_testing_.has_value()) {
    duration = base::Seconds(animation_duration_for_testing_.value());
  } else {
    double duration_in_milliseconds =
        kImpulseMillisecondsPerPixel * std::abs(MaximumDimension(delta));
    duration_in_milliseconds = std::clamp(
        duration_in_milliseconds, kImpulseMinDurationMs, kImpulseMaxDurationMs);
    duration = base::Milliseconds(duration_in_milliseconds);
  }

  duration -= delayed_by;
  return (duration >= base::TimeDelta()) ? duration : base::TimeDelta();
}

void ScrollOffsetAnimationCurve::SetInitialValue(
    const gfx::PointF& initial_value,
    base::TimeDelta delayed_by,
    float velocity) {
  initial_value_ = initial_value;
  has_set_initial_value_ = true;

  gfx::Vector2dF delta = target_value_ - initial_value;
  total_animation_duration_ = SegmentDuration(delta, delayed_by, velocity);
}

bool ScrollOffsetAnimationCurve::HasSetInitialValue() const {
  return has_set_initial_value_;
}

void ScrollOffsetAnimationCurve::ApplyAdjustment(
    const gfx::Vector2dF& adjustment) {
  initial_value_ = initial_value_ + adjustment;
  target_value_ = target_value_ + adjustment;
}

gfx::PointF ScrollOffsetAnimationCurve::GetValue(base::TimeDelta t) const {
  const base::TimeDelta duration = total_animation_duration_ - last_retarget_;
  t -= last_retarget_;

  if (duration.is_zero() || (t >= duration))
    return target_value_;
  if (t <= base::TimeDelta())
    return initial_value_;

  const double progress = timing_function_->GetValue(
      t / duration, TimingFunction::LimitDirection::RIGHT);
  return gfx::PointF(gfx::Tween::FloatValueBetween(progress, initial_value_.x(),
                                                   target_value_.x()),
                     gfx::Tween::FloatValueBetween(progress, initial_value_.y(),
                                                   target_value_.y()));
}

base::TimeDelta ScrollOffsetAnimationCurve::Duration() const {
  return total_animation_duration_;
}

int ScrollOffsetAnimationCurve::Type() const {
  return AnimationCurve::SCROLL_OFFSET;
}

const char* ScrollOffsetAnimationCurve::TypeName() const {
  return "ScrollOffset";
}

std::unique_ptr<gfx::AnimationCurve> ScrollOffsetAnimationCurve::Clone() const {
  return CloneToScrollOffsetAnimationCurve();
}

void ScrollOffsetAnimationCurve::Tick(
    base::TimeDelta t,
    int property_id,
    gfx::KeyframeModel* keyframe_model,
    gfx::TimingFunction::LimitDirection unused) const {
  if (target_) {
    target_->OnScrollOffsetAnimated(GetValue(t), property_id, keyframe_model);
  }
}

std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurve::CloneToScrollOffsetAnimationCurve() const {
  std::unique_ptr<TimingFunction> timing_function(
      static_cast<TimingFunction*>(timing_function_->Clone().release()));
  std::unique_ptr<ScrollOffsetAnimationCurve> curve_clone = base::WrapUnique(
      new ScrollOffsetAnimationCurve(target_value_, std::move(timing_function),
                                     animation_type_, duration_behavior_));
  curve_clone->initial_value_ = initial_value_;
  curve_clone->total_animation_duration_ = total_animation_duration_;
  curve_clone->last_retarget_ = last_retarget_;
  curve_clone->has_set_initial_value_ = has_set_initial_value_;
  return curve_clone;
}

void ScrollOffsetAnimationCurve::SetAnimationDurationForTesting(
    base::TimeDelta duration) {
  animation_duration_for_testing_ = duration.InSecondsF();
}

double ScrollOffsetAnimationCurve::CalculateVelocity(base::TimeDelta t) {
  base::TimeDelta duration = total_animation_duration_ - last_retarget_;
  const double slope =
      timing_function_->Velocity((t - last_retarget_) / duration);

  gfx::Vector2dF delta = target_value_ - initial_value_;

  // TimingFunction::Velocity just gives the slope of the curve. Convert it to
  // units of pixels per second.
  return slope * (MaximumDimension(delta) / duration.InSecondsF());
}

void ScrollOffsetAnimationCurve::UpdateTarget(base::TimeDelta t,
                                              const gfx::PointF& new_target) {
  DCHECK_NE(animation_type_, AnimationType::kLinear)
      << "UpdateTarget is not supported on linear scroll animations.";

  // UpdateTarget is still called for linear animations occasionally. This is
  // tracked via crbug.com/1164008.
  if (animation_type_ == AnimationType::kLinear)
    return;

  // If the new UpdateTarget actually happened before the previous one, keep
  // |t| as the most recent, but reduce the duration of any generated
  // animation.
  base::TimeDelta delayed_by = std::max(base::TimeDelta(), last_retarget_ - t);
  t = std::max(t, last_retarget_);

  if (animation_type_ == AnimationType::kEaseInOut &&
      std::abs(MaximumDimension(target_value_ - new_target)) < kEpsilon) {
    // Don't update the animation if the new target is the same as the old one.
    // This is done for EaseInOut-style animation curves, since the duration is
    // inversely proportional to the distance, and it may cause an animation
    // that is longer than the one currently running.
    // Specifically avoid doing this for Impulse-style animation curves since
    // its duration is directly proportional to the distance, and we don't want
    // to drop user input.
    target_value_ = new_target;
    return;
  }

  gfx::PointF current_position = GetValue(t);
  gfx::Vector2dF new_delta = new_target - current_position;

  // We are already at or very close to the new target. Stop animating.
  if (std::abs(MaximumDimension(new_delta)) < kEpsilon) {
    last_retarget_ = t;
    total_animation_duration_ = t;
    target_value_ = new_target;
    return;
  }

  // The last segment was of zero duration.
  base::TimeDelta old_duration = total_animation_duration_ - last_retarget_;
  if (old_duration.is_zero()) {
    DCHECK_EQ(t, last_retarget_);
    total_animation_duration_ = SegmentDuration(new_delta, delayed_by);
    target_value_ = new_target;
    return;
  }

  const base::TimeDelta new_duration =
      (animation_type_ == AnimationType::kEaseInOut)
          ? EaseInOutBoundedSegmentDuration(new_delta, t, delayed_by)
          : ImpulseSegmentDuration(new_delta, delayed_by);
  if (new_duration.InSecondsF() < kEpsilon) {
    // The duration is (close to) 0, so stop the animation.
    target_value_ = new_target;
    total_animation_duration_ = t;
    return;
  }

  // Adjust the slope of the new animation in order to preserve the velocity of
  // the old animation.
  double velocity = CalculateVelocity(t);
  double new_slope =
      velocity * (new_duration.InSecondsF() / MaximumDimension(new_delta));

  if (animation_type_ == AnimationType::kEaseInOut) {
    timing_function_ = EaseInOutWithInitialSlope(new_slope);
  } else {
    DCHECK_EQ(animation_type_, AnimationType::kImpulse);
    if (IsNewTargetInOppositeDirection(current_position, target_value_,
                                       new_target)) {
      // Prevent any rubber-banding by setting the velocity (and subsequently,
      // the slope) to 0 when moving in the opposite direciton.
      new_slope = 0;
    }
    timing_function_ = ImpulseCurveWithInitialSlope(new_slope);
  }

  initial_value_ = current_position;
  target_value_ = new_target;
  total_animation_duration_ = t + new_duration;
  last_retarget_ = t;
}

const ScrollOffsetAnimationCurve*
ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
    const AnimationCurve* c) {
  DCHECK_EQ(ScrollOffsetAnimationCurve::SCROLL_OFFSET, c->Type());
  return static_cast<const ScrollOffsetAnimationCurve*>(c);
}

ScrollOffsetAnimationCurve*
ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(AnimationCurve* c) {
  DCHECK_EQ(ScrollOffsetAnimationCurve::SCROLL_OFFSET, c->Type());
  return static_cast<ScrollOffsetAnimationCurve*>(c);
}

}  // namespace cc
