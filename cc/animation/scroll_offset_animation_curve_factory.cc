// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve_factory.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"

namespace cc {
namespace {
ScrollOffsetAnimationCurve::DurationBehavior GetDurationBehaviorFromScrollType(
    ScrollOffsetAnimationCurve::ScrollType scroll_type) {
  switch (scroll_type) {
    case ScrollOffsetAnimationCurve::ScrollType::kProgrammatic:
      return ScrollOffsetAnimationCurve::DurationBehavior::kDeltaBased;
    case ScrollOffsetAnimationCurve::ScrollType::kKeyboard:
      return ScrollOffsetAnimationCurve::DurationBehavior::kConstant;
    case ScrollOffsetAnimationCurve::ScrollType::kMouseWheel:
      return ScrollOffsetAnimationCurve::DurationBehavior::kInverseDelta;
    case ScrollOffsetAnimationCurve::ScrollType::kAutoScroll:
      NOTREACHED();
  }
}
}  // namespace

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateAnimation(
    const gfx::PointF& target_value,
    ScrollOffsetAnimationCurve::ScrollType scroll_type) {
  if (scroll_type == ScrollOffsetAnimationCurve::ScrollType::kAutoScroll) {
    return CreateLinearAnimation(target_value);
  }

  return CreateEaseInOutAnimation(
      target_value, scroll_type,
      GetDurationBehaviorFromScrollType(scroll_type));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
    const gfx::PointF& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return CreateEaseInOutAnimation(
      target_value, ScrollOffsetAnimationCurve::ScrollType::kProgrammatic,
      duration_behavior);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimationForTesting(
    const gfx::PointF& target_value) {
  return CreateLinearAnimation(target_value);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimation(
    const gfx::PointF& target_value,
    ScrollOffsetAnimationCurve::ScrollType scroll_type,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kEaseInOut,
      scroll_type, duration_behavior));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimation(
    const gfx::PointF& target_value) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kLinear,
      ScrollOffsetAnimationCurve::ScrollType::kAutoScroll));
}

}  // namespace cc
