// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_H_
#define CC_TREES_MUTATOR_HOST_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class MutatorEvents;
class MutatorHostClient;
class LayerTreeMutator;
class PropertyTrees;
class ScrollTree;

// Used as the return value of GetAnimationScales() to indicate that there is
// no active transform animation or the scale cannot be computed.
inline constexpr float kInvalidScale = 0.f;

// A MutatorHost owns all the animation and mutation effects.
// There is just one MutatorHost for LayerTreeHost on main renderer thread
// and just one MutatorHost for LayerTreeHostImpl on the impl thread.
// We synchronize them during the commit in a one-way data-flow process
// (PushPropertiesTo).
// A MutatorHost talks to its correspondent LayerTreeHost via
// MutatorHostClient interface.
class MutatorHost {
 public:
  virtual ~MutatorHost() = default;

  virtual std::unique_ptr<MutatorHost> CreateImplInstance() const = 0;

  virtual void ClearMutators() = 0;

  virtual void InitClientAnimationState() = 0;

  virtual void RemoveElementId(ElementId element_id) = 0;

  virtual void SetMutatorHostClient(MutatorHostClient* client) = 0;

  virtual void SetLayerTreeMutator(
      std::unique_ptr<LayerTreeMutator> mutator) = 0;

  virtual void PushPropertiesTo(MutatorHost* host_impl,
                                const PropertyTrees& property_trees) = 0;

  virtual void RemoveStaleTimelines() = 0;

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
  // Returns TIME_UPDATED events generated in this frame to be handled by
  // BeginMainFrame.
  virtual void TakeTimeUpdatedEvents(MutatorEvents* events) = 0;
  virtual void PromoteScrollTimelinesPendingToActive() = 0;

  virtual std::unique_ptr<MutatorEvents> CreateEvents() = 0;
  virtual void SetAnimationEvents(std::unique_ptr<MutatorEvents> events) = 0;

  virtual bool ScrollOffsetAnimationWasInterrupted(
      ElementId element_id) const = 0;

  virtual bool IsAnimatingProperty(ElementId element_id,
                                   ElementListType list_type,
                                   TargetProperty::Type property) const = 0;

  virtual bool HasPotentiallyRunningAnimationForProperty(
      ElementId element_id,
      ElementListType list_type,
      TargetProperty::Type property) const = 0;

  virtual bool HasAnyAnimationTargetingProperty(
      ElementId element_id,
      TargetProperty::Type property) const = 0;

  virtual bool AnimationsPreserveAxisAlignment(ElementId element_id) const = 0;

  // Returns the maximum scale along any dimension at any destination in active
  // scale animations, or kInvalidScale if there is no active transform
  // animation or the scale cannot be computed.
  virtual float MaximumScale(ElementId element_id,
                             ElementListType list_type) const = 0;

  virtual bool IsElementAnimating(ElementId element_id) const = 0;
  virtual bool HasTickingKeyframeModelForTesting(
      ElementId element_id) const = 0;

  virtual void ImplOnlyAutoScrollAnimationCreate(
      ElementId element_id,
      const gfx::PointF& target_offset,
      const gfx::PointF& current_offset,
      float autoscroll_velocity,
      base::TimeDelta animation_start_offset) = 0;

  virtual void ImplOnlyScrollAnimationCreate(
      ElementId element_id,
      const gfx::PointF& target_offset,
      const gfx::PointF& current_offset,
      base::TimeDelta delayed_by,
      base::TimeDelta animation_start_offset) = 0;
  virtual std::optional<gfx::PointF> ImplOnlyScrollAnimationUpdateTarget(
      const gfx::Vector2dF& scroll_delta,
      const gfx::PointF& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by,
      ElementId element_id) = 0;

  virtual void ScrollAnimationAbort(ElementId element_id) = 0;

  // Returns whether there is an ongoing scroll animation on Impl.
  virtual bool HasImplOnlyScrollAnimatingElement() const = 0;
  // Returns whether there is an ongoing auto-scroll animation on Impl.
  virtual bool HasImplOnlyAutoScrollAnimatingElement() const = 0;
  // Returns whether there is an ongoing scroll animation on the element
  // with the given id.
  virtual bool ElementHasImplOnlyScrollAnimation(ElementId) const = 0;
  // Discard animations on elements that have been removed from the layer tree.
  virtual void HandleRemovedScrollAnimatingElements(bool commits_to_active) = 0;

  virtual size_t MainThreadAnimationsCount() const = 0;
  virtual bool HasInvalidationAnimation() const = 0;
  virtual bool HasNativePropertyAnimation() const = 0;
  virtual bool CurrentFrameHadRAF() const = 0;
  virtual bool NextFrameHasPendingRAF() const = 0;
  virtual bool HasCanvasInvalidation() const = 0;
  virtual bool HasJSAnimation() const = 0;
  virtual bool HasSmilAnimation() const = 0;
  virtual bool HasViewTransition() const = 0;
  virtual bool HasScrollLinkedAnimation(ElementId for_scroller) const = 0;

  // Iterates through all animations and returns the minimum tick interval.
  // Returns 0 if there is a continuous animation which should be ticked
  // as fast as possible.
  virtual base::TimeDelta MinimumTickInterval() const = 0;

  using TrackedAnimationSequenceId = size_t;
  struct PendingThroughputTrackerInfo {
    // Id of a tracked animation sequence.
    TrackedAnimationSequenceId id = 0u;
    // True means the tracking for |id| is pending to start and false means
    // the tracking is pending to stop.
    bool start = false;
  };
  // Takes info of throughput trackers that are pending start or stop.
  using PendingThroughputTrackerInfos =
      std::vector<PendingThroughputTrackerInfo>;
  virtual PendingThroughputTrackerInfos TakePendingThroughputTrackerInfos() = 0;
};

class MutatorEvents {
 public:
  virtual ~MutatorEvents() = default;
  virtual bool IsEmpty() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_H_
