// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animation_curve_factory.h"

#include <memory>
#include "base/memory/ptr_util.h"
#include "cc/base/features.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace cc {
namespace {
ScrollOffsetAnimationCurve::DurationBehavior GetDurationBehaviorFromScrollType(
    ScrollOffsetAnimationCurveFactory::ScrollType scroll_type) {
  switch (scroll_type) {
    case ScrollOffsetAnimationCurveFactory::ScrollType::kProgrammatic:
      return ScrollOffsetAnimationCurve::DurationBehavior::DELTA_BASED;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kKeyboard:
      return ScrollOffsetAnimationCurve::DurationBehavior::CONSTANT;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kMouseWheel:
      return ScrollOffsetAnimationCurve::DurationBehavior::INVERSE_DELTA;
    case ScrollOffsetAnimationCurveFactory::ScrollType::kAutoScroll:
      NOTREACHED();
      return ScrollOffsetAnimationCurve::DurationBehavior::DELTA_BASED;
  }
}
}  // namespace

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateAnimation(
    const gfx::ScrollOffset& target_value,
    ScrollType scroll_type) {
  if (scroll_type == ScrollType::kAutoScroll)
    return CreateLinearAnimation(target_value);

  if (base::FeatureList::IsEnabled(features::kImpulseScrollAnimations))
    return CreateImpulseAnimation(target_value);

  return CreateEaseInOutAnimation(
      target_value, GetDurationBehaviorFromScrollType(scroll_type));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
    const gfx::ScrollOffset& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return CreateEaseInOutAnimation(target_value, duration_behavior);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimationForTesting(
    const gfx::ScrollOffset& target_value) {
  return CreateLinearAnimation(target_value);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateImpulseAnimationForTesting(
    const gfx::ScrollOffset& target_value) {
  return CreateImpulseAnimation(target_value);
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimation(
    const gfx::ScrollOffset& target_value,
    ScrollOffsetAnimationCurve::DurationBehavior duration_behavior) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kEaseInOut,
      duration_behavior));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateLinearAnimation(
    const gfx::ScrollOffset& target_value) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kLinear));
}

// static
std::unique_ptr<ScrollOffsetAnimationCurve>
ScrollOffsetAnimationCurveFactory::CreateImpulseAnimation(
    const gfx::ScrollOffset& target_value) {
  return base::WrapUnique(new ScrollOffsetAnimationCurve(
      target_value, ScrollOffsetAnimationCurve::AnimationType::kImpulse,
      ScrollOffsetAnimationCurve::DurationBehavior::INVERSE_DELTA));
}
}  // namespace cc
