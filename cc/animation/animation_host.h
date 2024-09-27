// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_HOST_H_
#define CC_ANIMATION_ANIMATION_HOST_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/keyframe_model.h"
#include "cc/base/protected_sequence_synchronizer.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class Animation;
class AnimationTimeline;
class ElementAnimations;
class LayerTreeHost;
class ScrollOffsetAnimations;
class ScrollOffsetAnimationsImpl;
class WorkletAnimation;

enum class ThreadInstance { kMain, kImpl };

// An AnimationHost contains all the state required to play animations.
// Specifically, it owns all the AnimationTimelines objects.
// There is just one AnimationHost for LayerTreeHost on main renderer thread
// and just one AnimationHost for LayerTreeHostImpl on impl thread.
// We synchronize them during the commit process in a one-way data flow process
// (PushPropertiesTo).
// An AnimationHost talks to its correspondent LayerTreeHost via
// MutatorHostClient interface.
class CC_ANIMATION_EXPORT AnimationHost : public MutatorHost,
                                          public LayerTreeMutatorClient,
                                          public ProtectedSequenceSynchronizer {
 public:
  using ElementToAnimationsMap =
      std::unordered_map<ElementId,
                         scoped_refptr<ElementAnimations>,
                         ElementIdHash>;
  using AnimationsList = std::vector<scoped_refptr<Animation>>;

  static std::unique_ptr<AnimationHost> CreateMainInstance();
  static std::unique_ptr<AnimationHost> CreateForTesting(
      ThreadInstance thread_instance);

  AnimationHost(const AnimationHost&) = delete;
  ~AnimationHost() override;

  AnimationHost& operator=(const AnimationHost&) = delete;

  using IdToTimelineMap =
      std::unordered_map<int, scoped_refptr<AnimationTimeline>>;
  const IdToTimelineMap& timelines() const {
    return id_to_timeline_map_.Read(*this);
  }

  void AddAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);
  void RemoveAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);

  // Lazy removal of an unused timeline.
  void DetachAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);

  const AnimationTimeline* GetTimelineById(int timeline_id) const;
  AnimationTimeline* GetTimelineById(int timeline_id);

  void RegisterAnimationForElement(ElementId element_id, Animation* animation);
  void UnregisterAnimationForElement(ElementId element_id,
                                     Animation* animation);
  void UpdateClientAnimationStateForElementAnimations(ElementId element_id);
  gfx::PointF GetScrollOffsetForAnimation(ElementId element_id) const;

  scoped_refptr<const ElementAnimations>
  GetElementAnimationsForElementIdForTesting(ElementId element_id) const;

  // Parent LayerTreeHost or LayerTreeHostImpl.
  MutatorHostClient* mutator_host_client() {
    DCHECK(IsOwnerThread() || InProtectedSequence());
    return mutator_host_client_;
  }
  const MutatorHostClient* mutator_host_client() const {
    DCHECK(IsOwnerThread() || InProtectedSequence());
    return mutator_host_client_;
  }

  // ProtectedSequenceSynchronizer implementation
  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

  void SetNeedsCommit();
  void SetNeedsPushProperties();
  void ResetNeedsPushProperties();
  bool needs_push_properties() const {
    return needs_push_properties_.Read(*this);
  }

  // MutatorHost implementation.
  std::unique_ptr<MutatorHost> CreateImplInstance() const override;
  void ClearMutators() override;
  base::TimeDelta MinimumTickInterval() const override;

  void InitClientAnimationState() override;

  void RemoveElementId(ElementId element_id) override;

  void SetMutatorHostClient(MutatorHostClient* client) override;

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator) override;

  void PushPropertiesTo(MutatorHost* host_impl,
                        const PropertyTrees& property_trees) override;

  void RemoveStaleTimelines() override;

  void SetScrollAnimationDurationForTesting(base::TimeDelta duration) override;
  bool NeedsTickAnimations() const override;

  bool ActivateAnimations(MutatorEvents* events) override;
  bool TickAnimations(base::TimeTicks monotonic_time,
                      const ScrollTree& scroll_tree,
                      bool is_active_tree) override;
  void TickScrollAnimations(base::TimeTicks monotonic_time,
                            const ScrollTree& scroll_tree) override;
  void TickWorkletAnimations() override;
  bool UpdateAnimationState(bool start_ready_animations,
                            MutatorEvents* events) override;
  void TakeTimeUpdatedEvents(MutatorEvents* events) override;
  // Should be called when the pending tree is promoted to active, as this may
  // require updating the ElementId for the ScrollTimeline scroll source.
  void PromoteScrollTimelinesPendingToActive() override;

  std::unique_ptr<MutatorEvents> CreateEvents() override;
  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events) override;

  bool ScrollOffsetAnimationWasInterrupted(ElementId element_id) const override;

  bool IsAnimatingProperty(ElementId element_id,
                           ElementListType list_type,
                           TargetProperty::Type property) const override;

  bool HasPotentiallyRunningAnimationForProperty(
      ElementId element_id,
      ElementListType list_type,
      TargetProperty::Type property) const override;

  bool HasAnyAnimationTargetingProperty(
      ElementId element_id,
      TargetProperty::Type property) const override;

  bool AnimationsPreserveAxisAlignment(ElementId element_id) const override;

  float MaximumScale(ElementId element_id,
                     ElementListType list_type) const override;

  bool IsElementAnimating(ElementId element_id) const override;
  bool HasTickingKeyframeModelForTesting(ElementId element_id) const override;

  void ImplOnlyAutoScrollAnimationCreate(
      ElementId element_id,
      const gfx::PointF& target_offset,
      const gfx::PointF& current_offset,
      float autoscroll_velocity,
      base::TimeDelta animation_start_offset) override;

  void ImplOnlyScrollAnimationCreate(
      ElementId element_id,
      const gfx::PointF& target_offset,
      const gfx::PointF& current_offset,
      base::TimeDelta delayed_by,
      base::TimeDelta animation_start_offset) override;
  std::optional<gfx::PointF> ImplOnlyScrollAnimationUpdateTarget(
      const gfx::Vector2dF& scroll_delta,
      const gfx::PointF& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by,
      ElementId element_id) override;

  void ScrollAnimationAbort(ElementId element_id) override;

  bool HasImplOnlyScrollAnimatingElement() const override;
  bool HasImplOnlyAutoScrollAnimatingElement() const override;
  bool ElementHasImplOnlyScrollAnimation(ElementId) const override;
  bool IsElementInPropertyTrees(ElementId element_id,
                                bool commits_to_active) const;
  void HandleRemovedScrollAnimatingElements(bool commits_to_active) override;

  // This should only be called from the main thread.
  ScrollOffsetAnimations& scroll_offset_animations();

  // Registers the given animation as ticking. A ticking animation is one that
  // has a running keyframe model.
  void AddToTicking(scoped_refptr<Animation> animation);

  // Unregisters the given animation. When this happens, the animation will no
  // longer be ticked.
  void RemoveFromTicking(scoped_refptr<Animation> animation);

  const AnimationsList& ticking_animations_for_testing() const;
  const ElementToAnimationsMap& element_animations_for_testing() const;

  // LayerTreeMutatorClient.
  void SetMutationUpdate(
      std::unique_ptr<MutatorOutputState> output_state) override;

  size_t MainThreadAnimationsCount() const override;
  // Returns true if there is any animation that affects pending tree, such as
  // custom property animations via paint worklet.
  bool HasInvalidationAnimation() const override;
  // Returns true if there is any animation that affects active tree, such as
  // transform animation.
  bool HasNativePropertyAnimation() const override;
  bool CurrentFrameHadRAF() const override;
  bool NextFrameHasPendingRAF() const override;
  PendingThroughputTrackerInfos TakePendingThroughputTrackerInfos() override;
  bool HasCanvasInvalidation() const override;
  bool HasJSAnimation() const override;
  bool HasSmilAnimation() const override;
  bool HasViewTransition() const override;
  bool HasScrollLinkedAnimation(ElementId for_scroller) const override;

  // Starts/stops throughput tracking represented by |sequence_id|.
  void StartThroughputTracking(TrackedAnimationSequenceId sequence_id);
  void StopThroughputTracking(TrackedAnimationSequenceId sequnece_id);

  void SetAnimationCounts(size_t total_animations_count);
  void SetHasCanvasInvalidation(bool has_canvas_invalidation);
  void SetHasInlineStyleMutation(bool has_inline_style_mutation);
  void SetHasSmilAnimation(bool has_svg_smil_animation);
  void SetHasViewTransition(bool has_view_transition);
  void SetCurrentFrameHadRaf(bool current_frame_had_raf);
  void SetNextFrameHasPendingRaf(bool next_frame_has_pending_raf);

 private:
  explicit AnimationHost(ThreadInstance thread_instance);

  const ElementAnimations* GetElementAnimationsForElementId(
      ElementId element_id) const;
  scoped_refptr<ElementAnimations> GetElementAnimationsForElementId(
      ElementId element_id);

  void PushTimelinesToImplThread(AnimationHost* host_impl) const;
  void RemoveTimelinesFromImplThread(AnimationHost* host_impl) const;
  void PushPropertiesToImplThread(AnimationHost* host_impl);

  void EraseTimeline(scoped_refptr<AnimationTimeline> timeline);

  // Return true if there are any animations that get mutated.
  void TickMutator(base::TimeTicks monotonic_time,
                   const ScrollTree& scroll_tree,
                   bool is_active_tree);

  // Return the state representing all ticking worklet animations.
  std::unique_ptr<MutatorInputState> CollectWorkletAnimationsState(
      base::TimeTicks timeline_time,
      const ScrollTree& scroll_tree,
      bool is_active_tree);

  // Returns a pointer to a worklet animation by worklet animation id or null
  // if there is no match.
  WorkletAnimation* FindWorkletAnimation(WorkletAnimationId id);

  ProtectedSequenceReadable<ElementToAnimationsMap> element_to_animations_map_;
  ProtectedSequenceReadable<AnimationsList> ticking_animations_;

  // A list of all timelines which this host owns.
  ProtectedSequenceReadable<IdToTimelineMap> id_to_timeline_map_;

  // A list of IDs for detached timelines. A timeline may be detached on the
  // owner thread even during a protected sequence. These timelines are no
  // longer used and should be cleaned up at the next opportune moment.
  ProtectedSequenceForbidden<IdToTimelineMap> detached_timeline_map_;

  // AnimationHosts's ProtectedSequenceSynchronizer implementation is
  // implemented using this member. As such the various helpers can not be used
  // to protect access (otherwise we would get infinite recursion).
  raw_ptr<MutatorHostClient> mutator_host_client_ = nullptr;

  // This is only non-null within the call scope of PushPropertiesTo().
  raw_ptr<const PropertyTrees> property_trees_ = nullptr;

  // Exactly one of scroll_offset_animations_ and scroll_offset_animations_impl_
  // will be non-null for a given AnimationHost instance (the former if
  // thread_instance_ == ThreadInstance::MAIN, the latter if thread_instance_ ==
  // ThreadInstance::IMPL).
  ProtectedSequenceWritable<std::unique_ptr<ScrollOffsetAnimations>>
      scroll_offset_animations_;
  ProtectedSequenceReadable<std::unique_ptr<ScrollOffsetAnimationsImpl>>
      scroll_offset_animations_impl_;

  const ThreadInstance thread_instance_;

  ProtectedSequenceWritable<bool> needs_push_properties_{false};

  ProtectedSequenceReadable<std::unique_ptr<LayerTreeMutator>> mutator_;

  ProtectedSequenceReadable<size_t> main_thread_animations_count_{0};
  ProtectedSequenceReadable<bool> current_frame_had_raf_{false};
  ProtectedSequenceReadable<bool> next_frame_has_pending_raf_{false};
  ProtectedSequenceReadable<bool> has_canvas_invalidation_{false};
  ProtectedSequenceReadable<bool> has_inline_style_mutation_{false};
  ProtectedSequenceReadable<bool> has_smil_animation_{false};
  ProtectedSequenceReadable<bool> has_view_transition_{false};

  ProtectedSequenceWritable<PendingThroughputTrackerInfos>
      pending_throughput_tracker_infos_;

  base::WeakPtrFactory<AnimationHost> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_HOST_H_
