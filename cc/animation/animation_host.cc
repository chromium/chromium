// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animations.h"
#include "cc/animation/scroll_offset_animations_impl.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/worklet_animation.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

namespace {

AnimationWorkletMutationState ToAnimationWorkletMutationState(
    MutateStatus status) {
  switch (status) {
    case MutateStatus::kCompletedWithUpdate:
      return AnimationWorkletMutationState::COMPLETED_WITH_UPDATE;

    case MutateStatus::kCompletedNoUpdate:
      return AnimationWorkletMutationState::COMPLETED_NO_UPDATE;

    case MutateStatus::kCanceled:
      return AnimationWorkletMutationState::CANCELED;
  }
}

}  // namespace

std::unique_ptr<AnimationHost> AnimationHost::CreateMainInstance() {
  return base::WrapUnique(new AnimationHost(ThreadInstance::MAIN));
}

std::unique_ptr<AnimationHost> AnimationHost::CreateForTesting(
    ThreadInstance thread_instance) {
  auto animation_host = base::WrapUnique(new AnimationHost(thread_instance));

  if (thread_instance == ThreadInstance::IMPL)
    animation_host->SetSupportsScrollAnimations(true);

  return animation_host;
}

AnimationHost::AnimationHost(ThreadInstance thread_instance)
    : mutator_host_client_(nullptr),
      thread_instance_(thread_instance),
      supports_scroll_animations_(false),
      needs_push_properties_(false),
      mutator_(nullptr) {
  if (thread_instance_ == ThreadInstance::IMPL) {
    scroll_offset_animations_impl_ =
        std::make_unique<ScrollOffsetAnimationsImpl>(this);
  } else {
    scroll_offset_animations_ = std::make_unique<ScrollOffsetAnimations>(this);
  }
}

AnimationHost::~AnimationHost() {
  scroll_offset_animations_impl_ = nullptr;

  ClearMutators();
  DCHECK(!mutator_host_client());
  DCHECK(element_to_animations_map_.empty());
}

std::unique_ptr<MutatorHost> AnimationHost::CreateImplInstance(
    bool supports_impl_scrolling) const {
  DCHECK_EQ(thread_instance_, ThreadInstance::MAIN);

  auto mutator_host_impl =
      base::WrapUnique<MutatorHost>(new AnimationHost(ThreadInstance::IMPL));
  mutator_host_impl->SetSupportsScrollAnimations(supports_impl_scrolling);
  return mutator_host_impl;
}

AnimationTimeline* AnimationHost::GetTimelineById(int timeline_id) const {
  auto f = id_to_timeline_map_.find(timeline_id);
  return f == id_to_timeline_map_.end() ? nullptr : f->second.get();
}

void AnimationHost::ClearMutators() {
  for (auto& kv : id_to_timeline_map_)
    EraseTimeline(kv.second);
  id_to_timeline_map_.clear();
}

void AnimationHost::EraseTimeline(scoped_refptr<AnimationTimeline> timeline) {
  timeline->ClearAnimations();
  timeline->SetAnimationHost(nullptr);
}

void AnimationHost::AddAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  DCHECK(timeline->id());
  timeline->SetAnimationHost(this);
  id_to_timeline_map_.insert(
      std::make_pair(timeline->id(), std::move(timeline)));
  SetNeedsPushProperties();
}

void AnimationHost::RemoveAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  DCHECK(timeline->id());
  EraseTimeline(timeline);
  id_to_timeline_map_.erase(timeline->id());
  SetNeedsPushProperties();
}

void AnimationHost::SetHasCanvasInvalidation(bool has_canvas_invalidation) {
  has_canvas_invalidation_ = has_canvas_invalidation;
}

bool AnimationHost::HasCanvasInvalidation() const {
  return has_canvas_invalidation_;
}

bool AnimationHost::HasJSAnimation() const {
  return has_inline_style_mutation_;
}

void AnimationHost::SetHasInlineStyleMutation(bool has_inline_style_mutation) {
  has_inline_style_mutation_ = has_inline_style_mutation;
}

void AnimationHost::UpdateRegisteredElementIds(ElementListType changed_list) {
  for (auto map_entry : element_to_animations_map_) {
    // kReservedElementId is reserved for an paint worklet element that animates
    // a custom property. This element is assumed to always be present as no
    // element is needed to tick this animation.
    if (mutator_host_client()->IsElementInPropertyTrees(map_entry.first,
                                                        changed_list) ||
        map_entry.first.GetStableId() == ElementId::kReservedElementId) {
      map_entry.second->ElementIdRegistered(map_entry.first, changed_list);
    } else {
      map_entry.second->ElementIdUnregistered(map_entry.first, changed_list);
    }
  }
}

void AnimationHost::InitClientAnimationState() {
  for (auto map_entry : element_to_animations_map_)
    map_entry.second->InitClientAnimationState();
}

void AnimationHost::RegisterElementId(ElementId element_id,
                                      ElementListType list_type) {
  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (element_animations)
    element_animations->ElementIdRegistered(element_id, list_type);
}

void AnimationHost::UnregisterElementId(ElementId element_id,
                                        ElementListType list_type) {
  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (element_animations)
    element_animations->ElementIdUnregistered(element_id, list_type);
}

void AnimationHost::RegisterAnimationForElement(ElementId element_id,
                                                Animation* animation) {
  DCHECK(element_id);
  DCHECK(animation);

  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (!element_animations) {
    element_animations = ElementAnimations::Create(this, element_id);
    element_to_animations_map_[element_animations->element_id()] =
        element_animations;
  }

  DCHECK(element_animations->AnimationHostIs(this));

  element_animations->AddKeyframeEffect(animation->keyframe_effect());
}

void AnimationHost::UnregisterAnimationForElement(ElementId element_id,
                                                  Animation* animation) {
  DCHECK(element_id);
  DCHECK(animation);

  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  DCHECK(element_animations);

  // |ClearAffectedElementTypes| requires an ElementId map in order to update
  // the property trees. Generating that map requires walking the keyframe
  // effects, so we have to do it before removing this one.
  PropertyToElementIdMap element_id_map =
      element_animations->GetPropertyToElementIdMap();

  element_animations->RemoveKeyframeEffect(animation->keyframe_effect());

  if (element_animations->IsEmpty()) {
    element_animations->ClearAffectedElementTypes(element_id_map);
    element_to_animations_map_.erase(element_animations->element_id());
    element_animations->ClearAnimationHost();
  }

  RemoveFromTicking(animation);
}

void AnimationHost::SetMutatorHostClient(MutatorHostClient* client) {
  if (mutator_host_client_ == client)
    return;

  mutator_host_client_ = client;
  if (mutator_host_client_ && needs_push_properties_)
    mutator_host_client_->SetMutatorsNeedCommit();
}

void AnimationHost::SetNeedsCommit() {
  DCHECK(mutator_host_client_);
  mutator_host_client_->SetMutatorsNeedCommit();
  // TODO(loyso): Invalidate property trees only if really needed.
  mutator_host_client_->SetMutatorsNeedRebuildPropertyTrees();
}

void AnimationHost::SetNeedsPushProperties() {
  if (needs_push_properties_)
    return;
  needs_push_properties_ = true;
  if (mutator_host_client_)
    mutator_host_client_->SetMutatorsNeedCommit();
}

void AnimationHost::PushPropertiesTo(MutatorHost* mutator_host_impl) {
  auto* host_impl = static_cast<AnimationHost*>(mutator_host_impl);

  // Update animation counts and whether raf was requested. These explicitly
  // do not request push properties and are pushed as part of the next commit
  // when it happens as requesting a commit leads to performance issues:
  // https://crbug.com/1083244
  host_impl->main_thread_animations_count_ = main_thread_animations_count_;
  host_impl->current_frame_had_raf_ = current_frame_had_raf_;
  host_impl->next_frame_has_pending_raf_ = next_frame_has_pending_raf_;

  if (needs_push_properties_) {
    needs_push_properties_ = false;
    PushTimelinesToImplThread(host_impl);
    RemoveTimelinesFromImplThread(host_impl);
    PushPropertiesToImplThread(host_impl);
    // This is redundant but used in tests.
    host_impl->needs_push_properties_ = false;
  }
}

void AnimationHost::PushTimelinesToImplThread(AnimationHost* host_impl) const {
  for (auto& kv : id_to_timeline_map_) {
    auto& timeline = kv.second;
    AnimationTimeline* timeline_impl =
        host_impl->GetTimelineById(timeline->id());
    if (timeline_impl)
      continue;

    scoped_refptr<AnimationTimeline> to_add = timeline->CreateImplInstance();
    host_impl->AddAnimationTimeline(to_add.get());
  }
}

void AnimationHost::RemoveTimelinesFromImplThread(
    AnimationHost* host_impl) const {
  IdToTimelineMap& timelines_impl = host_impl->id_to_timeline_map_;

  // Erase all the impl timelines which |this| doesn't have.
  for (auto it = timelines_impl.begin(); it != timelines_impl.end();) {
    auto& timeline_impl = it->second;
    if (timeline_impl->is_impl_only() || GetTimelineById(timeline_impl->id())) {
      ++it;
    } else {
      host_impl->EraseTimeline(it->second);
      it = timelines_impl.erase(it);
    }
  }
}

void AnimationHost::PushPropertiesToImplThread(AnimationHost* host_impl) {
  // Sync all animations with impl thread to create ElementAnimations. This
  // needs to happen before the element animations are synced below.
  for (auto& kv : id_to_timeline_map_) {
    AnimationTimeline* timeline = kv.second.get();
    if (AnimationTimeline* timeline_impl =
            host_impl->GetTimelineById(timeline->id())) {
      timeline->PushPropertiesTo(timeline_impl);
    }
  }

  // Sync properties for created ElementAnimations.
  for (auto& kv : element_to_animations_map_) {
    const auto& element_animations = kv.second;
    if (auto element_animations_impl =
            host_impl->GetElementAnimationsForElementId(kv.first)) {
      element_animations->PushPropertiesTo(std::move(element_animations_impl));
    }
  }

  // Update the impl-only scroll offset animations.
  scroll_offset_animations_->PushPropertiesTo(
      host_impl->scroll_offset_animations_impl_.get());

  // The pending info list is cleared in LayerTreeHostImpl::CommitComplete
  // and should be empty when pushing properties.
  DCHECK(host_impl->pending_throughput_tracker_infos_.empty());
  host_impl->pending_throughput_tracker_infos_ =
      TakePendingThroughputTrackerInfos();
}

scoped_refptr<ElementAnimations>
AnimationHost::GetElementAnimationsForElementId(ElementId element_id) const {
  if (!element_id)
    return nullptr;
  auto iter = element_to_animations_map_.find(element_id);
  return iter == element_to_animations_map_.end() ? nullptr : iter->second;
}

void AnimationHost::SetSupportsScrollAnimations(
    bool supports_scroll_animations) {
  supports_scroll_animations_ = supports_scroll_animations;
}

void AnimationHost::SetScrollAnimationDurationForTesting(
    base::TimeDelta duration) {
  ScrollOffsetAnimationCurve::SetAnimationDurationForTesting(duration);
}

bool AnimationHost::SupportsScrollAnimations() const {
  return supports_scroll_animations_;
}

bool AnimationHost::NeedsTickAnimations() const {
  return !ticking_animations_.empty();
}

void AnimationHost::TickMutator(base::TimeTicks monotonic_time,
                                const ScrollTree& scroll_tree,
                                bool is_active_tree) {
  if (!mutator_ || !mutator_->HasMutators())
    return;

  std::unique_ptr<MutatorInputState> state = CollectWorkletAnimationsState(
      monotonic_time, scroll_tree, is_active_tree);
  if (state->IsEmpty())
    return;

  ElementListType tree_type =
      is_active_tree ? ElementListType::ACTIVE : ElementListType::PENDING;

  auto on_done = base::BindOnce(
      [](base::WeakPtr<AnimationHost> animation_host, ElementListType tree_type,
         MutateStatus status) {
        if (animation_host->mutator_host_client_) {
          animation_host->mutator_host_client_
              ->NotifyAnimationWorkletStateChange(
                  ToAnimationWorkletMutationState(status), tree_type);
        }
      },
      weak_factory_.GetWeakPtr(), tree_type);

  MutateQueuingStrategy queuing_strategy =
      is_active_tree ? MutateQueuingStrategy::kQueueAndReplaceNormalPriority
                     : MutateQueuingStrategy::kQueueHighPriority;
  if (mutator_->Mutate(std::move(state), queuing_strategy,
                       std::move(on_done))) {
    mutator_host_client_->NotifyAnimationWorkletStateChange(
        AnimationWorkletMutationState::STARTED, tree_type);
  }
  return;
}

bool AnimationHost::ActivateAnimations(MutatorEvents* mutator_events) {
  if (!NeedsTickAnimations())
    return false;

  auto* animation_events = static_cast<AnimationEvents*>(mutator_events);

  TRACE_EVENT0("cc", "AnimationHost::ActivateAnimations");
  AnimationsList ticking_animations_copy = ticking_animations_;
  for (auto& it : ticking_animations_copy) {
    it->ActivateKeyframeModels();
    // Finish animations which no longer affect active or pending elements.
    it->UpdateState(false, animation_events);
  }

  return true;
}

bool AnimationHost::TickAnimations(base::TimeTicks monotonic_time,
                                   const ScrollTree& scroll_tree,
                                   bool is_active_tree) {
  TRACE_EVENT0("cc", "AnimationHost::TickAnimations");
  // We tick animations in the following order:
  // 1. regular animations 2. mutator 3. worklet animations
  //
  // Mutator may depend on scroll offset as its time input e.g., when there is
  // a worklet animation attached to a scroll timeline.
  // This ordering ensures we use the latest scroll offset as the input to the
  // mutator even if there are active scroll animations.
  // The ticking of worklet animations is deferred until draw to ensure that
  // mutator output takes effect in the same impl frame that it was mutated.
  if (!NeedsTickAnimations())
    return false;

  TRACE_EVENT_INSTANT0("cc", "NeedsTickAnimations", TRACE_EVENT_SCOPE_THREAD);

  bool animated = false;
  for (auto& kv : id_to_timeline_map_) {
    AnimationTimeline* timeline = kv.second.get();
    if (timeline->IsScrollTimeline()) {
      animated |= timeline->TickScrollLinkedAnimations(
          ticking_animations_, scroll_tree, is_active_tree);
    } else {
      animated |= timeline->TickTimeLinkedAnimations(ticking_animations_,
                                                     monotonic_time);
    }
  }

  // TODO(majidvp): At the moment we call this for both active and pending
  // trees similar to other animations. However our final goal is to only call
  // it once, ideally after activation, and only when the input
  // to an active timeline has changed. http://crbug.com/767210
  // Note that the TickMutator does not set the animated flag since these
  // mutations are processed asynchronously. Additional actions required to
  // handle these mutations are performed on receiving the asynchronous results.
  TickMutator(monotonic_time, scroll_tree, is_active_tree);

  return animated;
}

void AnimationHost::TickScrollAnimations(base::TimeTicks monotonic_time,
                                         const ScrollTree& scroll_tree) {
  // TODO(majidvp): We need to return a boolean here so that LTHI knows
  // whether it needs to schedule another frame.
  TickMutator(monotonic_time, scroll_tree, true /* is_active_tree */);
}

void AnimationHost::TickWorkletAnimations() {
  for (auto& animation : ticking_animations_) {
    if (!animation->IsWorkletAnimation())
      continue;
    animation->Tick(base::TimeTicks());
  }
}

std::unique_ptr<MutatorInputState> AnimationHost::CollectWorkletAnimationsState(
    base::TimeTicks monotonic_time,
    const ScrollTree& scroll_tree,
    bool is_active_tree) {
  TRACE_EVENT0("cc", "AnimationHost::CollectWorkletAnimationsState");
  std::unique_ptr<MutatorInputState> result =
      std::make_unique<MutatorInputState>();

  for (auto& animation : ticking_animations_) {
    if (!animation->IsWorkletAnimation())
      continue;

    ToWorkletAnimation(animation.get())
        ->UpdateInputState(result.get(), monotonic_time, scroll_tree,
                           is_active_tree);
  }

  return result;
}

bool AnimationHost::UpdateAnimationState(bool start_ready_animations,
                                         MutatorEvents* mutator_events) {
  if (!NeedsTickAnimations())
    return false;

  auto* animation_events = static_cast<AnimationEvents*>(mutator_events);

  TRACE_EVENT0("cc", "AnimationHost::UpdateAnimationState");
  AnimationsList ticking_animations_copy = ticking_animations_;
  for (auto& it : ticking_animations_copy)
    it->UpdateState(start_ready_animations, animation_events);

  return true;
}

void AnimationHost::TakeTimeUpdatedEvents(MutatorEvents* events) {
  auto* animation_events = static_cast<AnimationEvents*>(events);
  if (!animation_events->needs_time_updated_events())
    return;

  for (auto& it : ticking_animations_)
    it->TakeTimeUpdatedEvent(animation_events);

  animation_events->set_needs_time_updated_events(false);
}

void AnimationHost::PromoteScrollTimelinesPendingToActive() {
  for (auto& kv : id_to_timeline_map_) {
    auto& timeline = kv.second;
    timeline->ActivateTimeline();
  }
}

std::unique_ptr<MutatorEvents> AnimationHost::CreateEvents() {
  return std::make_unique<AnimationEvents>();
}

void AnimationHost::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> mutator_events) {
  DCHECK_EQ(thread_instance_, ThreadInstance::MAIN);
  auto events =
      base::WrapUnique(static_cast<AnimationEvents*>(mutator_events.release()));

  for (size_t event_index = 0; event_index < events->events_.size();
       ++event_index) {
    AnimationEvent& event = events->events_[event_index];
    AnimationTimeline* timeline = GetTimelineById(event.uid.timeline_id);
    if (timeline) {
      Animation* animation = timeline->GetAnimationById(event.uid.animation_id);
      if (animation)
        animation->DispatchAndDelegateAnimationEvent(event);
    }
  }
}

bool AnimationHost::ScrollOffsetAnimationWasInterrupted(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->ScrollOffsetAnimationWasInterrupted()
             : false;
}

bool AnimationHost::IsAnimatingFilterProperty(ElementId element_id,
                                              ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsCurrentlyAnimatingProperty(
                   TargetProperty::FILTER, list_type)
             : false;
}

bool AnimationHost::IsAnimatingBackdropFilterProperty(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->IsCurrentlyAnimatingProperty(
                                  TargetProperty::BACKDROP_FILTER, list_type)
                            : false;
}

bool AnimationHost::IsAnimatingOpacityProperty(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsCurrentlyAnimatingProperty(
                   TargetProperty::OPACITY, list_type)
             : false;
}

bool AnimationHost::IsAnimatingTransformProperty(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsCurrentlyAnimatingProperty(
                   TargetProperty::TRANSFORM, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningFilterAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::FILTER, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningBackdropFilterAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::BACKDROP_FILTER, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningOpacityAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::OPACITY, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningTransformAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::TRANSFORM, list_type)
             : false;
}

bool AnimationHost::HasAnyAnimationTargetingProperty(
    ElementId element_id,
    TargetProperty::Type property) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  if (!element_animations)
    return false;

  return element_animations->HasAnyAnimationTargetingProperty(property);
}

bool AnimationHost::AnimationsPreserveAxisAlignment(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->AnimationsPreserveAxisAlignment()
             : true;
}

void AnimationHost::GetAnimationScales(ElementId element_id,
                                       ElementListType list_type,
                                       float* maximum_scale,
                                       float* starting_scale) const {
  if (auto element_animations = GetElementAnimationsForElementId(element_id)) {
    element_animations->GetAnimationScales(list_type, maximum_scale,
                                           starting_scale);
    return;
  }
  *maximum_scale = kNotScaled;
  *starting_scale = kNotScaled;
}

bool AnimationHost::IsElementAnimating(ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->HasAnyKeyframeModel() : false;
}

bool AnimationHost::HasTickingKeyframeModelForTesting(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->HasTickingKeyframeEffect()
                            : false;
}

void AnimationHost::ImplOnlyAutoScrollAnimationCreate(
    ElementId element_id,
    const gfx::ScrollOffset& target_offset,
    const gfx::ScrollOffset& current_offset,
    float autoscroll_velocity,
    base::TimeDelta animation_start_offset) {
  DCHECK(scroll_offset_animations_impl_);
  scroll_offset_animations_impl_->AutoScrollAnimationCreate(
      element_id, target_offset, current_offset, autoscroll_velocity,
      animation_start_offset);
}

void AnimationHost::ImplOnlyScrollAnimationCreate(
    ElementId element_id,
    const gfx::ScrollOffset& target_offset,
    const gfx::ScrollOffset& current_offset,
    base::TimeDelta delayed_by,
    base::TimeDelta animation_start_offset) {
  DCHECK(scroll_offset_animations_impl_);
  scroll_offset_animations_impl_->MouseWheelScrollAnimationCreate(
      element_id, target_offset, current_offset, delayed_by,
      animation_start_offset);
}

bool AnimationHost::ImplOnlyScrollAnimationUpdateTarget(
    const gfx::Vector2dF& scroll_delta,
    const gfx::ScrollOffset& max_scroll_offset,
    base::TimeTicks frame_monotonic_time,
    base::TimeDelta delayed_by) {
  DCHECK(scroll_offset_animations_impl_);
  return scroll_offset_animations_impl_->ScrollAnimationUpdateTarget(
      scroll_delta, max_scroll_offset, frame_monotonic_time, delayed_by);
}

ScrollOffsetAnimations& AnimationHost::scroll_offset_animations() const {
  DCHECK(scroll_offset_animations_);
  return *scroll_offset_animations_.get();
}

void AnimationHost::ScrollAnimationAbort() {
  DCHECK(scroll_offset_animations_impl_);
  scroll_offset_animations_impl_->ScrollAnimationAbort(
      false /* needs_completion */);
}

ElementId AnimationHost::ImplOnlyScrollAnimatingElement() const {
  DCHECK(scroll_offset_animations_impl_);
  if (!scroll_offset_animations_impl_->IsAnimating())
    return ElementId();

  return scroll_offset_animations_impl_->GetElementId();
}

void AnimationHost::AddToTicking(scoped_refptr<Animation> animation) {
  DCHECK(!base::Contains(ticking_animations_, animation));
  ticking_animations_.push_back(animation);
}

void AnimationHost::RemoveFromTicking(scoped_refptr<Animation> animation) {
  auto to_erase = std::find(ticking_animations_.begin(),
                            ticking_animations_.end(), animation);
  if (to_erase != ticking_animations_.end())
    ticking_animations_.erase(to_erase);
}

const AnimationHost::AnimationsList&
AnimationHost::ticking_animations_for_testing() const {
  return ticking_animations_;
}

const AnimationHost::ElementToAnimationsMap&
AnimationHost::element_animations_for_testing() const {
  return element_to_animations_map_;
}

void AnimationHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  if (mutator == mutator_)
    return;
  mutator_ = std::move(mutator);
  mutator_->SetClient(this);
}

WorkletAnimation* AnimationHost::FindWorkletAnimation(WorkletAnimationId id) {
  // TODO(majidvp): Use a map to make lookup O(1)
  auto animation = std::find_if(
      ticking_animations_.begin(), ticking_animations_.end(), [id](auto& it) {
        return it->IsWorkletAnimation() &&
               ToWorkletAnimation(it.get())->worklet_animation_id() == id;
      });

  if (animation == ticking_animations_.end())
    return nullptr;

  return ToWorkletAnimation(animation->get());
}

void AnimationHost::SetMutationUpdate(
    std::unique_ptr<MutatorOutputState> output_state) {
  if (!output_state)
    return;

  TRACE_EVENT0("cc", "AnimationHost::SetMutationUpdate");
  for (auto& animation_state : output_state->animations) {
    WorkletAnimationId id = animation_state.worklet_animation_id;

    WorkletAnimation* to_update = FindWorkletAnimation(id);
    if (to_update)
      to_update->SetOutputState(animation_state);
  }
}

void AnimationHost::SetAnimationCounts(
    size_t total_animations_count,
    bool current_frame_had_raf,
    bool next_frame_has_pending_raf) {
  // Though these changes are pushed as part of AnimationHost::PushPropertiesTo
  // we don't SetNeedsPushProperties as pushing the values requires a commit.
  // Instead we allow them to be pushed whenever the next required commit
  // happens to avoid unnecessary work. See https://crbug.com/1083244.

  // If an animation is being run on the compositor, it will have a ticking
  // Animation (which will have a corresponding impl-thread version). Therefore
  // to find the count of main-only animations, we can simply subtract the
  // number of ticking animations from the total count.
  size_t ticking_animations_count = ticking_animations_.size();
  main_thread_animations_count_ =
      total_animations_count - ticking_animations_count;
  DCHECK_GE(main_thread_animations_count_, 0u);
  current_frame_had_raf_ = current_frame_had_raf;
  next_frame_has_pending_raf_ = next_frame_has_pending_raf;
}

size_t AnimationHost::MainThreadAnimationsCount() const {
  return main_thread_animations_count_;
}

bool AnimationHost::HasCustomPropertyAnimations() const {
  for (const auto& it : ticking_animations_)
    if (it->AffectsCustomProperty())
      return true;
  return false;
}

bool AnimationHost::CurrentFrameHadRAF() const {
  return current_frame_had_raf_;
}

bool AnimationHost::NextFrameHasPendingRAF() const {
  return next_frame_has_pending_raf_;
}

AnimationHost::PendingThroughputTrackerInfos
AnimationHost::TakePendingThroughputTrackerInfos() {
  PendingThroughputTrackerInfos infos =
      std::move(pending_throughput_tracker_infos_);
  pending_throughput_tracker_infos_ = {};
  return infos;
}

void AnimationHost::StartThroughputTracking(
    TrackedAnimationSequenceId sequence_id) {
  pending_throughput_tracker_infos_.push_back({sequence_id, true});
  SetNeedsPushProperties();
}

void AnimationHost::StopThroughputTracking(
    TrackedAnimationSequenceId sequnece_id) {
  pending_throughput_tracker_infos_.push_back({sequnece_id, false});
  SetNeedsPushProperties();
}

}  // namespace cc
