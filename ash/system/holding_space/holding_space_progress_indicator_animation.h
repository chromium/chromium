// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_ANIMATION_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_ANIMATION_H_

#include <memory>

#include "base/callback_list.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace ash {

// An animation for a `HoldingSpaceProgressIndicator`.
class HoldingSpaceProgressIndicatorAnimation : public gfx::AnimationDelegate {
 public:
  HoldingSpaceProgressIndicatorAnimation(
      const HoldingSpaceProgressIndicatorAnimation&) = delete;
  HoldingSpaceProgressIndicatorAnimation& operator=(
      const HoldingSpaceProgressIndicatorAnimation&) = delete;
  ~HoldingSpaceProgressIndicatorAnimation() override;

  // Adds the specified `callback` to be notified of animation updates. The
  // `callback` will continue to receive events so long as both `this` and the
  // returned subscription exist.
  base::CallbackListSubscription AddAnimationUpdatedCallback(
      base::RepeatingClosureList::CallbackType callback);

  // Immediately starts this animation.
  void Start();

  // Returns whether this animation is currently running.
  bool IsAnimating() const;

  // Returns the time at which this animation was `Start()`-ed.
  base::TimeTicks start_time() const { return start_time_; }

 protected:
  HoldingSpaceProgressIndicatorAnimation(base::TimeDelta duration,
                                         bool is_cyclic);

  // Implementing classes should update any desired animatable properties as
  // appropriate for the specified animation `fraction`.
  virtual void UpdateAnimatableProperties(double fraction) = 0;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Immediately start this animation. If `is_cyclic_restart` is `true`, this
  // animation is being restarted after completion of a full animation cycle.
  void StartInternal(bool is_cyclic_restart);

  // The duration for this animation.
  const base::TimeDelta duration_;

  // Whether this animation should loop on completion.
  const bool is_cyclic_;

  // The underlying animator which drives animation progress.
  std::unique_ptr<gfx::SlideAnimation> animator_;

  // The time at which this animation was `Start()`-ed.
  base::TimeTicks start_time_;

  // The list of callbacks for which to notify animation updates.
  base::RepeatingClosureList animation_updated_callback_list_;

  base::WeakPtrFactory<HoldingSpaceProgressIndicatorAnimation> weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_INDICATOR_ANIMATION_H_
