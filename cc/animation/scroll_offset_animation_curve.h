// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_

#include <memory>

#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

class TimingFunction;

// ScrollOffsetAnimationCurve computes scroll offset as a function of time
// during a scroll offset animation.
//
// Scroll offset animations can run either in Blink or in cc, in response to
// user input or programmatic scroll operations.  For more information about
// scheduling and servicing scroll animations, see blink::ScrollAnimator and
// blink::ProgrammaticScrollAnimator.

class CC_ANIMATION_EXPORT ScrollOffsetAnimationCurve : public AnimationCurve {
 public:
  // Indicates how the animation duration should be computed.
  enum class DurationBehavior {
    // Duration proportional to scroll delta; used for programmatic scrolls.
    DELTA_BASED,
    // Constant duration; used for keyboard scrolls.
    CONSTANT,
    // Duration inversely proportional to scroll delta within certain bounds.
    // Used for mouse wheels, makes fast wheel flings feel "snappy" while
    // preserving smoothness of slow wheel movements.
    INVERSE_DELTA,
    // Constant velocity; used for autoscrolls.
    CONSTANT_VELOCITY,
  };
  static std::unique_ptr<ScrollOffsetAnimationCurve> Create(
      const gfx::ScrollOffset& target_value,
      std::unique_ptr<TimingFunction> timing_function,
      DurationBehavior = DurationBehavior::DELTA_BASED);

  static base::TimeDelta SegmentDuration(const gfx::Vector2dF& delta,
                                         DurationBehavior behavior,
                                         base::TimeDelta delayed_by,
                                         float velocity);

  ScrollOffsetAnimationCurve(const ScrollOffsetAnimationCurve&) = delete;
  ~ScrollOffsetAnimationCurve() override;

  ScrollOffsetAnimationCurve& operator=(const ScrollOffsetAnimationCurve&) =
      delete;

  // Sets the initial offset and velocity (in pixels per second).
  void SetInitialValue(const gfx::ScrollOffset& initial_value,
                       base::TimeDelta delayed_by = base::TimeDelta(),
                       float velocity = 0);
  bool HasSetInitialValue() const;
  gfx::ScrollOffset GetValue(base::TimeDelta t) const;
  gfx::ScrollOffset target_value() const { return target_value_; }

  // Updates the current curve to aim at a new target, starting at time t
  // relative to the start of the animation.  The duration is recomputed based
  // on the DurationBehavior the curve was constructed with.  The timing
  // function is an ease-in-out cubic bezier modified to preserve velocity at t.
  void UpdateTarget(base::TimeDelta t, const gfx::ScrollOffset& new_target);

  // Shifts the entire curve by a delta without affecting its shape or timing.
  // Used for scroll anchoring adjustments that happen during scroll animations
  // (see blink::ScrollAnimator::AdjustAnimationAndSetScrollOffset).
  void ApplyAdjustment(const gfx::Vector2dF& adjustment);

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  CurveType Type() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;
  std::unique_ptr<ScrollOffsetAnimationCurve>
  CloneToScrollOffsetAnimationCurve() const;

  static void SetAnimationDurationForTesting(base::TimeDelta duration);

 private:
  ScrollOffsetAnimationCurve(const gfx::ScrollOffset& target_value,
                             std::unique_ptr<TimingFunction> timing_function,
                             DurationBehavior);

  gfx::ScrollOffset initial_value_;
  gfx::ScrollOffset target_value_;
  base::TimeDelta total_animation_duration_;

  // Time from animation start to most recent UpdateTarget.
  base::TimeDelta last_retarget_;

  std::unique_ptr<TimingFunction> timing_function_;
  DurationBehavior duration_behavior_;

  bool has_set_initial_value_;

  static base::Optional<double> animation_duration_for_testing_;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATION_CURVE_H_
