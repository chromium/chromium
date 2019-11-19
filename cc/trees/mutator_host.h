// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_H_
#define CC_TREES_MUTATOR_HOST_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class MutatorEvents;
class MutatorHostClient;
class LayerTreeMutator;
class ScrollTree;

// Used as the return value of GetAnimationScales() to indicate that there is
// no active scale animation or the scale cannot be computed.
const float kNotScaled = 0;

// A MutatorHost owns all the animation and mutation effects.
// There is just one MutatorHost for LayerTreeHost on main renderer thread
// and just one MutatorHost for LayerTreeHostImpl on the impl thread.
// We synchronize them during the commit in a one-way data-flow process
// (PushPropertiesTo).
// A MutatorHost talks to its correspondent LayerTreeHost via
// MutatorHostClient interface.
class MutatorHost {
 public:
  virtual ~MutatorHost() {}

  virtual std::unique_ptr<MutatorHost> CreateImplInstance(
      bool supports_impl_scrolling) const = 0;

  virtual void ClearMutators() = 0;

  virtual void UpdateRegisteredElementIds(ElementListType changed_list) = 0;
  virtual void InitClientAnimationState() = 0;

  virtual void RegisterElementId(ElementId element_id,
                                 ElementListType list_type) = 0;
  virtual void UnregisterElementId(ElementId element_id,
                                   ElementListType list_type) = 0;

  virtual void SetMutatorHostClient(MutatorHostClient* client) = 0;

  virtual void SetLayerTreeMutator(
      std::unique_ptr<LayerTreeMutator> mutator) = 0;

  virtual void PushPropertiesTo(MutatorHost* host_impl) = 0;

  virtual void SetSupportsScrollAnimations(bool supports_scroll_animations) = 0;
  virtual void SetScrollAnimationDurationForTesting(
      base::TimeDelta duration) = 0;
  virtual bool NeedsTickAnimations() const = 0;

  virtual bool ActivateAnimations(MutatorEvents* events) = 0;
  // TODO(smcgruer): Once we only tick scroll-based animations on scroll, we
  // don't need to pass the scroll tree in here.
  virtual bool TickAnimations(base::TimeTicks monotonic_time,
                              const ScrollTree& scroll_tree,
                              bool is_active_tree) = 0;
  // Tick animations that depends on scroll offset.
  virtual void TickScrollAnimations(base::TimeTicks monotonic_time,
                                    const ScrollTree& scroll_tree) = 0;
  virtual void TickWorkletAnimations() = 0;
  virtual bool UpdateAnimationState(bool start_ready_animations,
                                    MutatorEvents* events) = 0;
  virtual void PromoteScrollTimelinesPendingToActive() = 0;

  virtual std::unique_ptr<MutatorEvents> CreateEvents() = 0;
  virtual void SetAnimationEvents(std::unique_ptr<MutatorEvents> events) = 0;

  virtual bool ScrollOffsetAnimationWasInterrupted(
      ElementId element_id) const = 0;

  virtual bool IsAnimatingFilterProperty(ElementId element_id,
                                         ElementListType list_type) const = 0;
  virtual bool IsAnimatingBackdropFilterProperty(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool IsAnimatingOpacityProperty(ElementId element_id,
                                          ElementListType list_type) const = 0;
  virtual bool IsAnimatingTransformProperty(
      ElementId element_id,
      ElementListType list_type) const = 0;

  virtual bool HasPotentiallyRunningFilterAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningBackdropFilterAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningOpacityAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningTransformAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;

  virtual bool HasAnyAnimationTargetingProperty(
      ElementId element_id,
      TargetProperty::Type property) const = 0;

  virtual bool AnimationsPreserveAxisAlignment(ElementId element_id) const = 0;

  // Gets scales transform animations. On return, |maximum_scale| is the maximum
  // scale along any dimension at any destination in active scale animations,
  // and |starting_scale| is the maximum of starting animation scale along any
  // dimension at any destination in active scale animations. They are set to
  // kNotScaled if there is no active scale animation or the scales cannot be
  // computed.
  virtual void GetAnimationScales(ElementId element_id,
                                  ElementListType list_type,
                                  float* maximum_scale,
                                  float* starting_scale) const = 0;

  virtual bool IsElementAnimating(ElementId element_id) const = 0;
  virtual bool HasTickingKeyframeModelForTesting(
      ElementId element_id) const = 0;

  virtual void ImplOnlyAutoScrollAnimationCreate(
      ElementId element_id,
      const gfx::ScrollOffset& target_offset,
      const gfx::ScrollOffset& current_offset,
      float autoscroll_velocity,
      base::TimeDelta animation_start_offset) = 0;

  virtual void ImplOnlyScrollAnimationCreate(
      ElementId element_id,
      const gfx::ScrollOffset& target_offset,
      const gfx::ScrollOffset& current_offset,
      base::TimeDelta delayed_by,
      base::TimeDelta animation_start_offset) = 0;
  virtual bool ImplOnlyScrollAnimationUpdateTarget(
      ElementId element_id,
      const gfx::Vector2dF& scroll_delta,
      const gfx::ScrollOffset& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by) = 0;

  virtual void ScrollAnimationAbort() = 0;

  // True when there is an ongoing scroll animation on Impl.
  virtual bool IsImplOnlyScrollAnimating() const = 0;

  virtual size_t CompositedAnimationsCount() const = 0;
  virtual size_t MainThreadAnimationsCount() const = 0;
  virtual bool HasCustomPropertyAnimations() const = 0;
  virtual bool CurrentFrameHadRAF() const = 0;
  virtual bool NextFrameHasPendingRAF() const = 0;
};

class MutatorEvents {
 public:
  virtual ~MutatorEvents() {}
  virtual bool IsEmpty() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_H_
