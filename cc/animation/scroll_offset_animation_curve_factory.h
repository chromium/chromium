// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_FACTORY_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_FACTORY_H_

#include <memory>

#include "cc/animation/scroll_offset_animation_curve.h"

namespace cc {
class CC_ANIMATION_EXPORT ScrollOffsetAnimationCurveFactory {
 public:
  enum class ScrollType { kProgrammatic, kKeyboard, kMouseWheel, kAutoScroll };

  static std::unique_ptr<ScrollOffsetAnimationCurve> CreateAnimation(
      const gfx::PointF& target_value,
      ScrollType scroll_type);

  static std::unique_ptr<ScrollOffsetAnimationCurve>
  CreateEaseInOutAnimationForTesting(
      const gfx::PointF& target_value,
      ScrollOffsetAnimationCurve::DurationBehavior duration_behavior =
          ScrollOffsetAnimationCurve::DurationBehavior::kDeltaBased);

  static std::unique_ptr<ScrollOffsetAnimationCurve>
  CreateLinearAnimationForTesting(const gfx::PointF& target_value);

  static std::unique_ptr<ScrollOffsetAnimationCurve>
  CreateImpulseAnimationForTesting(const gfx::PointF& target_value);

 private:
  static std::unique_ptr<ScrollOffsetAnimationCurve> CreateEaseInOutAnimation(
      const gfx::PointF& target_value,
      ScrollOffsetAnimationCurve::DurationBehavior duration_hint);

  static std::unique_ptr<ScrollOffsetAnimationCurve> CreateLinearAnimation(
      const gfx::PointF& target_value);

  static std::unique_ptr<ScrollOffsetAnimationCurve> CreateImpulseAnimation(
      const gfx::PointF& target_value);
};
}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_FACTORY_H_
