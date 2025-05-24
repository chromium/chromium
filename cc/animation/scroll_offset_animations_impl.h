// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_
#define CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class Animation;
class AnimationHost;
class AnimationTimeline;

// This class represents a scroll offset animation that is managed by only the
// impl thread, i.e. an impl-only scroll animation. It contains an
// AnimationTimeline and owns the (impl-only) scroll offset Animation running
// on a particular CC Layer. It exists only on the compositor thread.
class CC_ANIMATION_EXPORT ScrollOffsetAnimationImpl : public AnimationDelegate {
 public:
  explicit ScrollOffsetAnimationImpl(AnimationHost* animation_host);
  ScrollOffsetAnimationImpl(const ScrollOffsetAnimationImpl&) = delete;
  ~ScrollOffsetAnimationImpl() override;

  ScrollOffsetAnimationImpl& operator=(const ScrollOffsetAnimationImpl&) =
      delete;

  void AutoScrollAnimationCreate(ElementId element_id,
                                 const gfx::PointF& target_offset,
                                 const gfx::PointF& current_offset,
                                 float autoscroll_velocity,
                                 base::TimeDelta animation_start_offset);

  // |delayed_by| shrinks the duration of the
  // animation. |animation_start_offset| causes us to start the animation
  // partway through.
  void MouseWheelScrollAnimationCreate(ElementId element_id,
                                       const gfx::PointF& target_offset,
                                       const gfx::PointF& current_offset,
                                       base::TimeDelta delayed_by,
                                       base::TimeDelta animation_start_offset);

  std::optional<gfx::PointF> ScrollAnimationUpdateTarget(
      const gfx::Vector2dF& scroll_delta,
      const gfx::PointF& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by);

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
  void NotifyAnimationTakeover(
      base::TimeTicks monotonic_time,
      int target_property,
      base::TimeTicks animation_start_time,
      std::unique_ptr<gfx::AnimationCurve> curve) override {}
  void NotifyLocalTimeUpdated(
      std::optional<base::TimeDelta> local_time) override {}

  // Aborts the currently running scroll offset animation on an element and
  // starts a new one offsetted by adjustment.
  void ScrollAnimationApplyAdjustment(ElementId element_id,
                                      const gfx::Vector2dF& adjustment);

  void ScrollAnimationAbort(bool needs_completion);
  void AnimatingElementRemovedByCommit();

  bool IsAnimating() const;
  bool IsAutoScrolling() const;
  ElementId GetElementId() const;

 private:
  void ScrollAnimationCreateInternal(ElementId element_id,
                                     std::unique_ptr<gfx::AnimationCurve> curve,
                                     base::TimeDelta animation_start_offset);

  void ReattachScrollOffsetAnimationIfNeeded(ElementId element_id);

  raw_ptr<AnimationHost> animation_host_;
  scoped_refptr<AnimationTimeline> scroll_offset_timeline_;
  scoped_refptr<Animation> scroll_offset_animation_;
  bool animation_is_autoscroll_ = false;
};

// Pre-MultiImplScrollAnimations:
// Contains an ScrollOffsetAnimationImpl which encapsulates the scroll offset
// animation running on a particular CC Layer.
// We have just one animation for impl-only scroll offset animations. I.e. only
// one element can have an impl-only scroll offset animation at any given time.
//
// Post-MultiImplScrollAnimations:
// Contains a map from ElementId to ScrollOffsetAnimationImpl that owns the impl
// only scroll offset animation running on a particular CC Layer.
//
// Note that this class only exists on the compositor thread.
class CC_ANIMATION_EXPORT ScrollOffsetAnimationsImpl {
 public:
  explicit ScrollOffsetAnimationsImpl(AnimationHost* animation_host);
  ScrollOffsetAnimationsImpl(const ScrollOffsetAnimationsImpl&) = delete;
  ~ScrollOffsetAnimationsImpl();

  ScrollOffsetAnimationsImpl& operator=(const ScrollOffsetAnimationsImpl&) =
      delete;

  void AutoScrollAnimationCreate(ElementId element_id,
                                 const gfx::PointF& target_offset,
                                 const gfx::PointF& current_offset,
                                 float autoscroll_velocity,
                                 base::TimeDelta animation_start_offset);

  // |delayed_by| shrinks the duration of the
  // animation. |animation_start_offset| causes us to start the animation
  // partway through.
  void MouseWheelScrollAnimationCreate(ElementId element_id,
                                       const gfx::PointF& target_offset,
                                       const gfx::PointF& current_offset,
                                       base::TimeDelta delayed_by,
                                       base::TimeDelta animation_start_offset);

  std::optional<gfx::PointF> ScrollAnimationUpdateTarget(
      const gfx::Vector2dF& scroll_delta,
      const gfx::PointF& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by,
      ElementId element_id);

  // Aborts the currently running scroll offset animation on an element and
  // starts a new one offsetted by adjustment.
  void ScrollAnimationApplyAdjustment(ElementId element_id,
                                      const gfx::Vector2dF& adjustment);

  void ScrollAnimationAbort(bool needs_completion, ElementId element_id);

  void HandleRemovedScrollAnimatingElements(bool commits_to_active);

  bool ElementHasImplOnlyScrollAnimation(ElementId element_id) const;
  bool HasImplOnlyScrollAnimatingElement() const;
  bool HasImplOnlyAutoScrollAnimatingElement() const;

  bool IsAnimating() const;
  bool IsAutoScrolling() const;
  ElementId GetElementId() const;

 private:
  // This retrieves the ScrollOffsetAnimationImpl object associated with the
  // given ElementId. It is only used when MultiImplScrollAnimations is enabled.
  ScrollOffsetAnimationImpl* GetScrollAnimation(ElementId element_id) const;

  raw_ptr<AnimationHost> animation_host_;

  // We have just one animation for impl-only scroll offset animations.
  // I.e. only one element can have an impl-only scroll offset animation at
  // any given time.
  std::unique_ptr<ScrollOffsetAnimationImpl> scroll_offset_animation_;

  // This maps each animating scroll container's ElementId to a
  // ScrollOffsetAnimationImpl object. It is only used when
  // MultiImplScrollAnimations is enabled.
  base::flat_map<ElementId, std::unique_ptr<ScrollOffsetAnimationImpl>>
      element_to_animation_map_;
};

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_OFFSET_ANIMATIONS_IMPL_H_
