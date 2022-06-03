// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_ANIMATION_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_ANIMATION_H_

#include <memory>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

// An animation for a `HoldingSpaceProgressRing` to be painted in lieu of the
// determinate progress ring that would otherwise be painted.
class HoldingSpaceProgressRingAnimation : public gfx::AnimationDelegate {
 public:
  enum class Type {
    kIndeterminate,  // See `HoldingSpaceProgressRingIndeterminateAnimation`.
    kPulse,          // See `HoldingSpaceProgressRingPulseAnimation`.
  };

  HoldingSpaceProgressRingAnimation(const HoldingSpaceProgressRingAnimation&) =
      delete;
  HoldingSpaceProgressRingAnimation& operator=(
      const HoldingSpaceProgressRingAnimation&) = delete;
  ~HoldingSpaceProgressRingAnimation() override;

  // Returns a created progress ring animation of the specified `type`.
  static std::unique_ptr<HoldingSpaceProgressRingAnimation> CreateOfType(
      Type type);

  // Adds the specified `callback` to be notified of animation updates. The
  // `callback` will continue to receive events so long as both `this` and the
  // returned subscription exist.
  base::RepeatingClosureList::Subscription AddAnimationUpdatedCallback(
      base::RepeatingClosureList::CallbackType callback);

  // Immediately starts this animation.
  void Start();

  // Returns whether this animation is currently running.
  bool IsAnimating() const;

  Type type() const { return type_; }
  base::TimeTicks start_time() const { return start_time_; }

  // Returns animatable properties.
  float start_position() const { return start_position_; }
  float end_position() const { return end_position_; }
  float opacity() const { return opacity_; }

 protected:
  HoldingSpaceProgressRingAnimation(Type type,
                                    base::TimeDelta duration,
                                    bool is_cyclic);

  // Implementing classes should update any desired animatable properties as
  // appropriate for the specified animation `fraction`.
  virtual void UpdateAnimatableProperties(double fraction,
                                          float* start_position,
                                          float* end_position,
                                          float* opacity) = 0;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Immediately start this animation. If `is_cyclic_restart` is `true`, this
  // animation is being restarted after completion of a full animation cycle.
  void StartInternal(bool is_cyclic_restart);

  // The specific type of this animation.
  const Type type_;

  // The duration for this animation.
  const base::TimeDelta duration_;

  // Whether or not this animation should loop on completion.
  const bool is_cyclic_;

  // The underlying animator which drives animation progress.
  std::unique_ptr<gfx::SlideAnimation> animator_;

  // The time at which this animation was `Start()`-ed.
  base::TimeTicks start_time_;

  // Animatable properties.
  float start_position_ = 0.f;
  float end_position_ = 1.f;
  float opacity_ = 1.f;

  // The list of callbacks for which to notify animation updates.
  base::RepeatingClosureList animation_updated_callback_list_;

  base::WeakPtrFactory<HoldingSpaceProgressRingAnimation> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_RING_ANIMATION_H_
