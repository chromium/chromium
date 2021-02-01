// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_

#include "base/memory/ref_counted.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

class Animation;
class AnimationHost;
class AnimationTimeline;

// Contains an AnimationTimeline and its Animation that owns the impl
// only scroll offset animations running on a particular CC Layer.
// We have just one animation for impl-only scroll offset animations. I.e. only
// one element can have an impl-only scroll offset animation at any given time.
// Note that this class only exists on the compositor thread.
class CC_ANIMATION_EXPORT ScrollOffsetAnimationsImpl
    : public AnimationDelegate {
 public:
  explicit ScrollOffsetAnimationsImpl(AnimationHost* animation_host);
  ScrollOffsetAnimationsImpl(const ScrollOffsetAnimationsImpl&) = delete;
  ~ScrollOffsetAnimationsImpl() override;

  ScrollOffsetAnimationsImpl& operator=(const ScrollOffsetAnimationsImpl&) =
      delete;

  void AutoScrollAnimationCreate(ElementId element_id,
                                 const gfx::ScrollOffset& target_offset,
                                 const gfx::ScrollOffset& current_offset,
                                 float autoscroll_velocity,
                                 base::TimeDelta animation_start_offset);

  // |delayed_by| shrinks the duration of the
  // animation. |animation_start_offset| causes us to start the animation
  // partway through.
  void MouseWheelScrollAnimationCreate(ElementId element_id,
                                       const gfx::ScrollOffset& target_offset,
                                       const gfx::ScrollOffset& current_offset,
                                       base::TimeDelta delayed_by,
                                       base::TimeDelta animation_start_offset);

  bool ScrollAnimationUpdateTarget(const gfx::Vector2dF& scroll_delta,
                                   const gfx::ScrollOffset& max_scroll_offset,
                                   base::TimeTicks frame_monotonic_time,
                                   base::TimeDelta delayed_by);

  // Aborts the currently running scroll offset animation on an element and
  // starts a new one offsetted by adjustment.
  void ScrollAnimationApplyAdjustment(ElementId element_id,
                                      const gfx::Vector2dF& adjustment);

  void ScrollAnimationAbort(bool needs_completion);

  // AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {}
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override {}
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               int target_property,
                               base::TimeTicks animation_start_time,
                               std::unique_ptr<AnimationCurve> curve) override {
  }
  void NotifyLocalTimeUpdated(
      base::Optional<base::TimeDelta> local_time) override {}

  bool IsAnimating() const;
  ElementId GetElementId() const;

 private:
  void ScrollAnimationCreateInternal(ElementId element_id,
                                     std::unique_ptr<AnimationCurve> curve,
                                     base::TimeDelta animation_start_offset);

  void ReattachScrollOffsetAnimationIfNeeded(ElementId element_id);

  AnimationHost* animation_host_;
  scoped_refptr<AnimationTimeline> scroll_offset_timeline_;

  // We have just one animation for impl-only scroll offset animations.
  // I.e. only one element can have an impl-only scroll offset animation at
  // any given time.
  scoped_refptr<Animation> scroll_offset_animation_;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_
