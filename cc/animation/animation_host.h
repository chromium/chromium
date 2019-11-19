// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_HOST_H_
#define CC_ANIMATION_ANIMATION_HOST_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/keyframe_model.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class Animation;
class AnimationTimeline;
class ElementAnimations;
class LayerTreeHost;
class KeyframeEffect;
class ScrollOffsetAnimations;
class ScrollOffsetAnimationsImpl;
class WorkletAnimation;

enum class ThreadInstance { MAIN, IMPL };

// An AnimationHost contains all the state required to play animations.
// Specifically, it owns all the AnimationTimelines objects.
// There is just one AnimationHost for LayerTreeHost on main renderer thread
// and just one AnimationHost for LayerTreeHostImpl on impl thread.
// We synchronize them during the commit process in a one-way data flow process
// (PushPropertiesTo).
// An AnimationHost talks to its correspondent LayerTreeHost via
// MutatorHostClient interface.
class CC_ANIMATION_EXPORT AnimationHost : public MutatorHost,
                                          public LayerTreeMutatorClient {
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

  void AddAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);
  void RemoveAnimationTimeline(scoped_refptr<AnimationTimeline> timeline);
  AnimationTimeline* GetTimelineById(int timeline_id) const;

  void RegisterKeyframeEffectForElement(ElementId element_id,
                                        KeyframeEffect* keyframe_effect);
  void UnregisterKeyframeEffectForElement(ElementId element_id,
                                          KeyframeEffect* keyframe_effect);

  scoped_refptr<ElementAnimations> GetElementAnimationsForElementId(
      ElementId element_id) const;

  // Parent LayerTreeHost or LayerTreeHostImpl.
  MutatorHostClient* mutator_host_client() { return mutator_host_client_; }
  const MutatorHostClient* mutator_host_client() const {
    return mutator_host_client_;
  }

  void SetNeedsCommit();
  void SetNeedsPushProperties();
  bool needs_push_properties() const { return needs_push_properties_; }

  bool SupportsScrollAnimations() const;

  // MutatorHost implementation.
  std::unique_ptr<MutatorHost> CreateImplInstance(
      bool supports_impl_scrolling) const override;
  void ClearMutators() override;

  // Processes the current |element_to_animations_map_|, registering animations
  // which can now be animated and unregistering those that can't based on the
  // elements in the |changed_list|.
  void UpdateRegisteredElementIds(ElementListType changed_list) override;
  void InitClientAnimationState() override;

  void RegisterElementId(ElementId element_id,
                         ElementListType list_type) override;
  void UnregisterElementId(ElementId element_id,
                           ElementListType list_type) override;

  void SetMutatorHostClient(MutatorHostClient* client) override;

  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator) override;

  void PushPropertiesTo(MutatorHost* host_impl) override;

  void SetSupportsScrollAnimations(bool supports_scroll_animations) override;
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
  void PromoteScrollTimelinesPendingToActive() override;

  std::unique_ptr<MutatorEvents> CreateEvents() override;
  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events) override;

  bool ScrollOffsetAnimationWasInterrupted(ElementId element_id) const override;

  bool IsAnimatingFilterProperty(ElementId element_id,
                                 ElementListType list_type) const override;
  bool IsAnimatingBackdropFilterProperty(
      ElementId element_id,
      ElementListType list_type) const override;
  bool IsAnimatingOpacityProperty(ElementId element_id,
                                  ElementListType list_type) const override;
  bool IsAnimatingTransformProperty(ElementId element_id,
                                    ElementListType list_type) const override;

  bool HasPotentiallyRunningFilterAnimation(
      ElementId element_id,
      ElementListType list_type) const override;
  bool HasPotentiallyRunningBackdropFilterAnimation(
      ElementId element_id,
      ElementListType list_type) const override;
  bool HasPotentiallyRunningOpacityAnimation(
      ElementId element_id,
      ElementListType list_type) const override;
  bool HasPotentiallyRunningTransformAnimation(
      ElementId element_id,
      ElementListType list_type) const override;

  bool HasAnyAnimationTargetingProperty(
      ElementId element_id,
      TargetProperty::Type property) const override;

  bool AnimationsPreserveAxisAlignment(ElementId element_id) const override;

  void GetAnimationScales(ElementId element_id,
                          ElementListType list_type,
                          float* maximum_scale,
                          float* starting_scale) const override;

  bool IsElementAnimating(ElementId element_id) const override;
  bool HasTickingKeyframeModelForTesting(ElementId element_id) const override;

  void ImplOnlyAutoScrollAnimationCreate(
      ElementId element_id,
      const gfx::ScrollOffset& target_offset,
      const gfx::ScrollOffset& current_offset,
      float autoscroll_velocity,
      base::TimeDelta animation_start_offset) override;

  void ImplOnlyScrollAnimationCreate(
      ElementId element_id,
      const gfx::ScrollOffset& target_offset,
      const gfx::ScrollOffset& current_offset,
      base::TimeDelta delayed_by,
      base::TimeDelta animation_start_offset) override;
  bool ImplOnlyScrollAnimationUpdateTarget(
      ElementId element_id,
      const gfx::Vector2dF& scroll_delta,
      const gfx::ScrollOffset& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by) override;

  void ScrollAnimationAbort() override;

  bool IsImplOnlyScrollAnimating() const override;

  // This should only be called from the main thread.
  ScrollOffsetAnimations& scroll_offset_animations() const;

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

  size_t CompositedAnimationsCount() const override;
  size_t MainThreadAnimationsCount() const override;
  bool HasCustomPropertyAnimations() const override;
  bool CurrentFrameHadRAF() const override;
  bool NextFrameHasPendingRAF() const override;
  void SetAnimationCounts(size_t total_animations_count,
                          bool current_frame_had_raf,
                          bool next_frame_has_pending_raf);

 private:
  explicit AnimationHost(ThreadInstance thread_instance);

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

  ElementToAnimationsMap element_to_animations_map_;
  AnimationsList ticking_animations_;

  // A list of all timelines which this host owns.
  using IdToTimelineMap =
      std::unordered_map<int, scoped_refptr<AnimationTimeline>>;
  IdToTimelineMap id_to_timeline_map_;

  MutatorHostClient* mutator_host_client_;

  // Exactly one of scroll_offset_animations_ and scroll_offset_animations_impl_
  // will be non-null for a given AnimationHost instance (the former if
  // thread_instance_ == ThreadInstance::MAIN, the latter if thread_instance_ ==
  // ThreadInstance::IMPL).
  std::unique_ptr<ScrollOffsetAnimations> scroll_offset_animations_;
  std::unique_ptr<ScrollOffsetAnimationsImpl> scroll_offset_animations_impl_;

  const ThreadInstance thread_instance_;

  bool supports_scroll_animations_;
  bool needs_push_properties_;

  std::unique_ptr<LayerTreeMutator> mutator_;

  size_t main_thread_animations_count_ = 0;
  bool current_frame_had_raf_ = false;
  bool next_frame_has_pending_raf_ = false;

  base::WeakPtrFactory<AnimationHost> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_HOST_H_
