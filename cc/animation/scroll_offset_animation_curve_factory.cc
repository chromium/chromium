// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve_factory.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "cc/base/features.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace cc {
namespace {
ScrollOffsetAnimationCurve::DurationBehavior GetDurationBehaviorFromScrollType(
    ScrollOffsetAnimationCurveFactory::ScrollType scroll_type) {
  switch (scroll_type) {
    case ScrollOffsetAnimationCurveFactory::ScrollType::kProgrammatic:
      return ScrollOffsetAnimationCurve::DurationBehavior::kDeltaBased;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kKeyboard:
      return ScrollOffsetAnimationCurve::DurationBehavior::kConstant;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kMouseWheel:
      return ScrollOffsetAnimationCurve::DurationBehavior::kInverseDelta;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kAutoScroll:
      NOTREACHED();
  }
}
}  // namespace

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateAnimation(
    const gfx::PointF& target_value,
    ScrollType scroll_type) {
  if (scroll_type == ScrollType::kAutoScroll)
    return CreateLinearAnimation(target_value);

  if (features::IsImpulseScrollAnimationEnabled())
    return CreateImpulseAnimation(target_value);

  return CreateEaseInOutAnimation(
      target_value, GetDurationBehaviorFromScrollType(scroll_type));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
    const gfx::PointF& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return CreateEaseInOutAnimation(target_value, duration_behavior);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimationForTesting(
    const gfx::PointF& target_value) {
  return CreateLinearAnimation(target_value);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateImpulseAnimationForTesting(
    const gfx::PointF& target_value) {
  return CreateImpulseAnimation(target_value);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimation(
    const gfx::PointF& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kEaseInOut,
      duration_behavior));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimation(
    const gfx::PointF& target_value) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kLinear));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateImpulseAnimation(
    const gfx::PointF& target_value) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kImpulse));
}
}  // namespace cc
