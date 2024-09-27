// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animations.h"
#include "cc/animation/scroll_offset_animations_impl.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/animation/worklet_animation.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

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
  return base::WrapUnique(new AnimationHost(ThreadInstance::kMain));
}

std::unique_ptr<AnimationHost> AnimationHost::CreateForTesting(
    ThreadInstance thread_instance) {
  auto animation_host = base::WrapUnique(new AnimationHost(thread_instance));

  return animation_host;
}

AnimationHost::AnimationHost(ThreadInstance thread_instance)
    : thread_instance_(thread_instance) {}

AnimationHost::~AnimationHost() {
  ClearMutators();
  DCHECK(!mutator_host_client());
}

std::unique_ptr<MutatorHost> AnimationHost::CreateImplInstance() const {
  DCHECK_EQ(thread_instance_, ThreadInstance::kMain);
  auto mutator_host_impl =
      base::WrapUnique<MutatorHost>(new AnimationHost(ThreadInstance::kImpl));
  return mutator_host_impl;
}

const AnimationTimeline* AnimationHost::GetTimelineById(int timeline_id) const {
  auto f = id_to_timeline_map_.Read(*this).find(timeline_id);
  return f == id_to_timeline_map_.Read(*this).end() ? nullptr : f->second.get();
}

AnimationTimeline* AnimationHost::GetTimelineById(int timeline_id) {
  auto f = id_to_timeline_map_.Write(*this).find(timeline_id);
  return f == id_to_timeline_map_.Write(*this).end() ? nullptr
                                                     : f->second.get();
}

void AnimationHost::ClearMutators() {
  for (auto& kv : id_to_timeline_map_.Read(*this))
    EraseTimeline(kv.second);
  id_to_timeline_map_.Write(*this).clear();
}

base::TimeDelta AnimationHost::MinimumTickInterval() const {
  base::TimeDelta min_interval = base::TimeDelta::Max();
  for (const auto& animation : ticking_animations_.Read(*this)) {
    DCHECK(animation->keyframe_effect());
    base::TimeDelta interval =
        animation->keyframe_effect()->MinimumTickInterval();
    if (interval.is_zero())
      return interval;
    if (interval < min_interval)
      min_interval = interval;
  }
  return min_interval;
}

void AnimationHost::EraseTimeline(scoped_refptr<AnimationTimeline> timeline) {
  timeline->ClearAnimations();
  timeline->SetAnimationHost(nullptr);
}

void AnimationHost::AddAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  DCHECK(timeline->id());
  id_to_timeline_map_.Write(*this).insert(
      std::make_pair(timeline->id(), timeline));
  timeline->SetAnimationHost(this);
  SetNeedsPushProperties();
}

void AnimationHost::RemoveAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  DCHECK(timeline->id());
  EraseTimeline(timeline);
  id_to_timeline_map_.Write(*this).erase(timeline->id());
  SetNeedsPushProperties();
}

void AnimationHost::DetachAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  if (InProtectedSequence()) {
    // Defer cleanup until post-commit.
    detached_timeline_map_.Write(*this).insert(
        std::make_pair(timeline->id(), timeline));
  } else {
    RemoveAnimationTimeline(timeline);
  }
}

void AnimationHost::SetHasCanvasInvalidation(bool has_canvas_invalidation) {
  has_canvas_invalidation_.Write(*this) = has_canvas_invalidation;
}

bool AnimationHost::HasCanvasInvalidation() const {
  return has_canvas_invalidation_.Read(*this);
}

bool AnimationHost::HasJSAnimation() const {
  return has_inline_style_mutation_.Read(*this);
}

void AnimationHost::SetHasInlineStyleMutation(bool has_inline_style_mutation) {
  has_inline_style_mutation_.Write(*this) = has_inline_style_mutation;
}

bool AnimationHost::HasSmilAnimation() const {
  return has_smil_animation_.Read(*this);
}

void AnimationHost::SetHasSmilAnimation(bool has_smil_animation) {
  has_smil_animation_.Write(*this) = has_smil_animation;
}

bool AnimationHost::HasViewTransition() const {
  return has_view_transition_.Read(*this);
}

void AnimationHost::SetHasViewTransition(bool has_view_transition) {
  has_view_transition_.Write(*this) = has_view_transition;
}

void AnimationHost::SetCurrentFrameHadRaf(bool current_frame_had_raf) {
  current_frame_had_raf_.Write(*this) = current_frame_had_raf;
}

bool AnimationHost::CurrentFrameHadRAF() const {
  return current_frame_had_raf_.Read(*this);
}

void AnimationHost::SetNextFrameHasPendingRaf(bool next_frame_has_pending_raf) {
  next_frame_has_pending_raf_.Write(*this) = next_frame_has_pending_raf;
}

bool AnimationHost::NextFrameHasPendingRAF() const {
  return next_frame_has_pending_raf_.Read(*this);
}

void AnimationHost::InitClientAnimationState() {
  for (auto map_entry : element_to_animations_map_.Write(*this))
    map_entry.second->InitClientAnimationState();
}

void AnimationHost::RemoveElementId(ElementId element_id) {
  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (element_animations) {
    DCHECK(!element_animations->HasTickingKeyframeEffect());
    element_animations->RemoveKeyframeEffects();
  }
}

void AnimationHost::RegisterAnimationForElement(ElementId element_id,
                                                Animation* animation) {
  DCHECK(element_id);
  DCHECK(animation);
#if DCHECK_IS_ON()
  for (const auto& keyframe_model :
       animation->keyframe_effect()->keyframe_models()) {
    KeyframeModel* cc_keyframe_model =
        KeyframeModel::ToCcKeyframeModel(keyframe_model.get());
    ElementId model_element_id = cc_keyframe_model->element_id()
                                     ? cc_keyframe_model->element_id()
                                     : element_id;
    DCHECK(cc_keyframe_model->affects_active_elements() ||
           cc_keyframe_model->affects_pending_elements());
    DCHECK(!cc_keyframe_model->affects_active_elements() ||
           mutator_host_client()->IsElementInPropertyTrees(
               model_element_id, ElementListType::ACTIVE));
    // Test thread_instance_ because LayerTreeHost has no pending tree.
    DCHECK(thread_instance_ == ThreadInstance::kMain ||
           !cc_keyframe_model->affects_pending_elements() ||
           mutator_host_client()->IsElementInPropertyTrees(
               model_element_id, ElementListType::PENDING));
  }
#endif

  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (!element_animations) {
    element_animations = ElementAnimations::Create(this, element_id);
    element_to_animations_map_.Write(*this)[element_animations->element_id()] =
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
    element_to_animations_map_.Write(*this).erase(
        element_animations->element_id());
    element_animations->ClearAnimationHost();
  }

  RemoveFromTicking(animation);
}

void AnimationHost::UpdateClientAnimationStateForElementAnimations(
    ElementId element_id) {
  auto* element_animations = GetElementAnimationsForElementId(element_id).get();
  if (element_animations)
    element_animations->UpdateClientAnimationState();
}

void AnimationHost::SetMutatorHostClient(MutatorHostClient* client) {
  if (mutator_host_client() == client)
    return;

  WaitForProtectedSequenceCompletion();

  if (!client) {
    scroll_offset_animations_impl_.Write(*this).reset();
    scroll_offset_animations_.Write(*this).reset();
    ClearMutators();
  }

  mutator_host_client_ = client;

  // Creating ScrollOffsetAnimationsImpl calls back into this, triggering
  // DCHECKs that are easier to verify once `mutator_host_client_` has been
  // set.
  if (mutator_host_client() && !scroll_offset_animations_impl_.Read(*this)) {
    if (thread_instance_ == ThreadInstance::kImpl) {
      scroll_offset_animations_impl_.Write(*this) =
          std::make_unique<ScrollOffsetAnimationsImpl>(this);
    } else {
      scroll_offset_animations_.Write(*this) =
          std::make_unique<ScrollOffsetAnimations>(this);
    }
  }

  if (mutator_host_client() && needs_push_properties_.Read(*this))
    mutator_host_client()->SetMutatorsNeedCommit();
}

bool AnimationHost::IsOwnerThread() const {
  return !mutator_host_client_ || mutator_host_client_->IsOwnerThread();
}

bool AnimationHost::InProtectedSequence() const {
  return !mutator_host_client_ || mutator_host_client_->InProtectedSequence();
}

void AnimationHost::WaitForProtectedSequenceCompletion() const {
  if (mutator_host_client_)
    mutator_host_client_->WaitForProtectedSequenceCompletion();
}

void AnimationHost::SetNeedsCommit() {
  DCHECK(mutator_host_client());
  DCHECK(IsOwnerThread());
  DCHECK(!InProtectedSequence());
  mutator_host_client()->SetMutatorsNeedCommit();
  // TODO(loyso): Invalidate property trees only if really needed.
  mutator_host_client()->SetMutatorsNeedRebuildPropertyTrees();
}

void AnimationHost::SetNeedsPushProperties() {
  if (needs_push_properties())
    return;
  needs_push_properties_.Write(*this) = true;
  if (mutator_host_client())
    mutator_host_client()->SetMutatorsNeedCommit();
}

void AnimationHost::ResetNeedsPushProperties() {
  needs_push_properties_.Write(*this) = false;
}

void AnimationHost::PushPropertiesTo(MutatorHost* mutator_host_impl,
                                     const PropertyTrees& property_trees) {
  auto* host_impl = static_cast<AnimationHost*>(mutator_host_impl);

  base::AutoReset<raw_ptr<const PropertyTrees>> properties(&property_trees_,
                                                           &property_trees);

  // Update animation counts and whether raf was requested. These explicitly
  // do not request push properties and are pushed as part of the next commit
  // when it happens as requesting a commit leads to performance issues:
  // https://crbug.com/1083244
  host_impl->main_thread_animations_count_.Write(*host_impl) =
      main_thread_animations_count_.Read(*this);
  host_impl->SetCurrentFrameHadRaf(CurrentFrameHadRAF());
  host_impl->SetNextFrameHasPendingRaf(NextFrameHasPendingRAF());
  host_impl->SetHasCanvasInvalidation(HasCanvasInvalidation());
  host_impl->SetHasInlineStyleMutation(HasJSAnimation());
  host_impl->SetHasSmilAnimation(HasSmilAnimation());
  host_impl->SetHasViewTransition(HasViewTransition());

  if (needs_push_properties()) {
    needs_push_properties_.Write(*this) = false;
    PushTimelinesToImplThread(host_impl);
    RemoveTimelinesFromImplThread(host_impl);
    PushPropertiesToImplThread(host_impl);

    // When using a display tree this ensures that any new animation updates are
    // pushed to Viz on next display tree update. When not using display trees,
    // setting this flag here is meaningless.
    host_impl->needs_push_properties_.Write(*host_impl) = true;
  }
}

void AnimationHost::RemoveStaleTimelines() {
  DCHECK(!InProtectedSequence());
  if (detached_timeline_map_.Read(*this).empty()) {
    return;
  }

  for (auto& kv : detached_timeline_map_.Read(*this)) {
    RemoveAnimationTimeline(kv.second);
  }
  detached_timeline_map_.Write(*this).clear();
}

void AnimationHost::PushTimelinesToImplThread(AnimationHost* host_impl) const {
  for (auto& kv : id_to_timeline_map_.Read(*this)) {
    auto& timeline = kv.second;
    const AnimationTimeline* timeline_impl =
        host_impl->GetTimelineById(timeline->id());
    if (timeline_impl)
      continue;

    scoped_refptr<AnimationTimeline> to_add = timeline->CreateImplInstance();
    host_impl->AddAnimationTimeline(std::move(to_add));
  }
}

void AnimationHost::RemoveTimelinesFromImplThread(
    AnimationHost* host_impl) const {
  IdToTimelineMap& timelines_impl =
      host_impl->id_to_timeline_map_.Write(*host_impl);

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
  base::AutoReset<raw_ptr<const PropertyTrees>> properties(
      &host_impl->property_trees_, property_trees_);

  // Sync all animations with impl thread to create ElementAnimations. This
  // needs to happen before the element animations are synced below.
  for (auto& kv : id_to_timeline_map_.Read(*this)) {
    AnimationTimeline* timeline = kv.second.get();
    if (AnimationTimeline* timeline_impl =
            host_impl->GetTimelineById(timeline->id())) {
      timeline->PushPropertiesTo(timeline_impl);
    }
  }

  // Sync properties for created ElementAnimations.
  for (auto& kv : element_to_animations_map_.Read(*this)) {
    const auto& element_animations = kv.second;
    if (auto element_animations_impl =
            host_impl->GetElementAnimationsForElementId(kv.first)) {
      element_animations->PushPropertiesTo(std::move(element_animations_impl));
    }
  }

  // Update the impl-only scroll offset animations.
  scroll_offset_animations_.Write(*this)->PushPropertiesTo(
      host_impl->scroll_offset_animations_impl_.Write(*host_impl).get());

  // The pending info list is cleared in LayerTreeHostImpl::CommitComplete
  // and should be empty when pushing properties.
  DCHECK(host_impl->pending_throughput_tracker_infos_.Read(*host_impl).empty());
  host_impl->pending_throughput_tracker_infos_.Write(*host_impl) =
      TakePendingThroughputTrackerInfos();
}

const ElementAnimations* AnimationHost::GetElementAnimationsForElementId(
    ElementId element_id) const {
  if (!element_id)
    return nullptr;
  auto iter = element_to_animations_map_.Read(*this).find(element_id);
  return iter == element_to_animations_map_.Read(*this).end()
             ? nullptr
             : iter->second.get();
}

scoped_refptr<ElementAnimations>
AnimationHost::GetElementAnimationsForElementId(ElementId element_id) {
  if (!element_id)
    return nullptr;
  auto iter = element_to_animations_map_.Write(*this).find(element_id);
  return iter == element_to_animations_map_.Write(*this).end() ? nullptr
                                                               : iter->second;
}

scoped_refptr<const ElementAnimations>
AnimationHost::GetElementAnimationsForElementIdForTesting(
    ElementId element_id) const {
  return GetElementAnimationsForElementId(element_id);
}

gfx::PointF AnimationHost::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  DCHECK(property_trees_);
  return property_trees_->scroll_tree().current_scroll_offset(element_id);
}

void AnimationHost::SetScrollAnimationDurationForTesting(
    base::TimeDelta duration) {
  ScrollOffsetAnimationCurve::SetAnimationDurationForTesting(duration);
}

bool AnimationHost::NeedsTickAnimations() const {
  for (auto& animation : ticking_animations_.Read(*this)) {
    if (!animation->keyframe_effect()->awaiting_deletion()) {
      return true;
    }
  }
  return false;
}

void AnimationHost::TickMutator(base::TimeTicks monotonic_time,
                                const ScrollTree& scroll_tree,
                                bool is_active_tree) {
  LayerTreeMutator* mutator = mutator_.Write(*this).get();
  if (!mutator || !mutator->HasMutators())
    return;

  DCHECK(IsOwnerThread());
  DCHECK(!InProtectedSequence());
  std::unique_ptr<MutatorInputState> state = CollectWorkletAnimationsState(
      monotonic_time, scroll_tree, is_active_tree);
  if (state->IsEmpty())
    return;

  ElementListType tree_type =
      is_active_tree ? ElementListType::ACTIVE : ElementListType::PENDING;

  auto on_done = base::BindOnce(
      [](base::WeakPtr<AnimationHost> animation_host, ElementListType tree_type,
         MutateStatus status) {
        if (animation_host->mutator_host_client()) {
          animation_host->mutator_host_client()
              ->NotifyAnimationWorkletStateChange(
                  ToAnimationWorkletMutationState(status), tree_type);
        }
      },
      weak_factory_.GetWeakPtr(), tree_type);

  MutateQueuingStrategy queuing_strategy =
      is_active_tree ? MutateQueuingStrategy::kQueueAndReplaceNormalPriority
                     : MutateQueuingStrategy::kQueueHighPriority;
  if (mutator->Mutate(std::move(state), queuing_strategy, std::move(on_done))) {
    mutator_host_client()->NotifyAnimationWorkletStateChange(
        AnimationWorkletMutationState::STARTED, tree_type);
  }
}

bool AnimationHost::ActivateAnimations(MutatorEvents* mutator_events) {
  if (!NeedsTickAnimations())
    return false;

  auto* animation_events = static_cast<AnimationEvents*>(mutator_events);

  TRACE_EVENT0("cc", "AnimationHost::ActivateAnimations");
  AnimationsList ticking_animations_copy = ticking_animations_.Read(*this);
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
  if (is_active_tree && !NeedsTickAnimations()) {
    return false;
  }

  TRACE_EVENT_INSTANT0("cc", "NeedsTickAnimations", TRACE_EVENT_SCOPE_THREAD);

  bool animated = false;
  std::vector<AnimationTimeline*> scroll_timelines;
  for (auto& kv : id_to_timeline_map_.Read(*this)) {
    AnimationTimeline* timeline = kv.second.get();
    if (timeline->IsScrollTimeline()) {
      scroll_timelines.push_back(timeline);
    } else {
      animated |= timeline->TickTimeLinkedAnimations(
          ticking_animations_.Read(*this), monotonic_time, !is_active_tree);
    }
  }
  // Tick the scroll-linked animations last, since a smooth scroll (time-linked)
  // might update the scroll offset.
  for (auto* timeline : scroll_timelines) {
    animated |= timeline->TickScrollLinkedAnimations(
        ticking_animations_.Read(*this), scroll_tree, is_active_tree);
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
  for (auto& animation : ticking_animations_.Read(*this)) {
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

  for (auto& animation : ticking_animations_.Read(*this)) {
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
  AnimationsList ticking_animations_copy = ticking_animations_.Read(*this);
  for (auto& it : ticking_animations_copy)
    it->UpdateState(start_ready_animations, animation_events);

  return true;
}

void AnimationHost::TakeTimeUpdatedEvents(MutatorEvents* events) {
  auto* animation_events = static_cast<AnimationEvents*>(events);
  if (!animation_events->needs_time_updated_events())
    return;

  for (auto& it : ticking_animations_.Read(*this))
    it->TakeTimeUpdatedEvent(animation_events);

  animation_events->set_needs_time_updated_events(false);
}

void AnimationHost::PromoteScrollTimelinesPendingToActive() {
  for (auto& kv : id_to_timeline_map_.Read(*this)) {
    auto& timeline = kv.second;
    timeline->ActivateTimeline();
  }
}

std::unique_ptr<MutatorEvents> AnimationHost::CreateEvents() {
  return std::make_unique<AnimationEvents>();
}

void AnimationHost::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> mutator_events) {
  DCHECK_EQ(thread_instance_, ThreadInstance::kMain);
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
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->ScrollOffsetAnimationWasInterrupted()
             : false;
}

bool AnimationHost::IsAnimatingProperty(ElementId element_id,
                                        ElementListType list_type,
                                        TargetProperty::Type property) const {
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->IsCurrentlyAnimatingProperty(
                                  property, list_type)
                            : false;
}

bool AnimationHost::HasPotentiallyRunningAnimationForProperty(
    ElementId element_id,
    ElementListType list_type,
    TargetProperty::Type property) const {
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(property,
                                                                  list_type)
             : false;
}

bool AnimationHost::HasAnyAnimationTargetingProperty(
    ElementId element_id,
    TargetProperty::Type property) const {
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  if (!element_animations)
    return false;

  return element_animations->HasAnyAnimationTargetingProperty(property,
                                                              element_id);
}

bool AnimationHost::AnimationsPreserveAxisAlignment(
    ElementId element_id) const {
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->AnimationsPreserveAxisAlignment()
             : true;
}

float AnimationHost::MaximumScale(ElementId element_id,
                                  ElementListType list_type) const {
  if (const auto* element_animations =
          GetElementAnimationsForElementId(element_id)) {
    return element_animations->MaximumScale(element_id, list_type);
  }
  return kInvalidScale;
}

bool AnimationHost::IsElementAnimating(ElementId element_id) const {
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->HasAnyKeyframeModel() : false;
}

bool AnimationHost::HasTickingKeyframeModelForTesting(
    ElementId element_id) const {
  const auto* element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->HasTickingKeyframeEffect()
                            : false;
}

void AnimationHost::ImplOnlyAutoScrollAnimationCreate(
    ElementId element_id,
    const gfx::PointF& target_offset,
    const gfx::PointF& current_offset,
    float autoscroll_velocity,
    base::TimeDelta animation_start_offset) {
  DCHECK(scroll_offset_animations_impl_.Read(*this));
  scroll_offset_animations_impl_.Write(*this)->AutoScrollAnimationCreate(
      element_id, target_offset, current_offset, autoscroll_velocity,
      animation_start_offset);
}

void AnimationHost::ImplOnlyScrollAnimationCreate(
    ElementId element_id,
    const gfx::PointF& target_offset,
    const gfx::PointF& current_offset,
    base::TimeDelta delayed_by,
    base::TimeDelta animation_start_offset) {
  DCHECK(scroll_offset_animations_impl_.Read(*this));
  scroll_offset_animations_impl_.Write(*this)->MouseWheelScrollAnimationCreate(
      element_id, target_offset, current_offset, delayed_by,
      animation_start_offset);
}

std::optional<gfx::PointF> AnimationHost::ImplOnlyScrollAnimationUpdateTarget(
    const gfx::Vector2dF& scroll_delta,
    const gfx::PointF& max_scroll_offset,
    base::TimeTicks frame_monotonic_time,
    base::TimeDelta delayed_by,
    ElementId element_id) {
  DCHECK(scroll_offset_animations_impl_.Read(*this));
  return scroll_offset_animations_impl_.Write(*this)
      ->ScrollAnimationUpdateTarget(scroll_delta, max_scroll_offset,
                                    frame_monotonic_time, delayed_by,
                                    element_id);
}

ScrollOffsetAnimations& AnimationHost::scroll_offset_animations() {
  DCHECK(scroll_offset_animations_.Read(*this));
  return *scroll_offset_animations_.Write(*this).get();
}

void AnimationHost::ScrollAnimationAbort(ElementId element_id) {
  DCHECK(scroll_offset_animations_impl_.Read(*this));
  scroll_offset_animations_impl_.Write(*this)->ScrollAnimationAbort(
      false /* needs_completion */, element_id);
}

bool AnimationHost::ElementHasImplOnlyScrollAnimation(
    ElementId element_id) const {
  return scroll_offset_animations_impl_.Read(*this)
      ->ElementHasImplOnlyScrollAnimation(element_id);
}

bool AnimationHost::HasImplOnlyScrollAnimatingElement() const {
  return scroll_offset_animations_impl_.Read(*this)
      ->HasImplOnlyScrollAnimatingElement();
}

bool AnimationHost::HasImplOnlyAutoScrollAnimatingElement() const {
  return scroll_offset_animations_impl_.Read(*this)
      ->HasImplOnlyAutoScrollAnimatingElement();
}

bool AnimationHost::IsElementInPropertyTrees(ElementId element_id,
                                             bool commits_to_active) const {
  return mutator_host_client()->IsElementInPropertyTrees(
      element_id,
      commits_to_active ? ElementListType::ACTIVE : ElementListType::PENDING);
}

void AnimationHost::HandleRemovedScrollAnimatingElements(
    bool commits_to_active) {
  scroll_offset_animations_impl_.Write(*this)
      ->HandleRemovedScrollAnimatingElements(commits_to_active);
}

void AnimationHost::AddToTicking(scoped_refptr<Animation> animation) {
  DCHECK(!base::Contains(ticking_animations_.Read(*this), animation));
  ticking_animations_.Write(*this).push_back(animation);
}

void AnimationHost::RemoveFromTicking(scoped_refptr<Animation> animation) {
  auto to_erase =
      base::ranges::find(ticking_animations_.Write(*this), animation);
  if (to_erase != ticking_animations_.Write(*this).end()) {
    ticking_animations_.Write(*this).erase(to_erase);
  }
}

const AnimationHost::AnimationsList&
AnimationHost::ticking_animations_for_testing() const {
  return ticking_animations_.Read(*this);
}

const AnimationHost::ElementToAnimationsMap&
AnimationHost::element_animations_for_testing() const {
  return element_to_animations_map_.Read(*this);
}

void AnimationHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  mutator_.Write(*this) = std::move(mutator);
  mutator_.Write(*this)->SetClient(this);
}

WorkletAnimation* AnimationHost::FindWorkletAnimation(WorkletAnimationId id) {
  // TODO(majidvp): Use a map to make lookup O(1)
  auto animation =
      base::ranges::find_if(ticking_animations_.Read(*this), [id](auto& it) {
        return it->IsWorkletAnimation() &&
               ToWorkletAnimation(it.get())->worklet_animation_id() == id;
      });

  if (animation == ticking_animations_.Read(*this).end())
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

void AnimationHost::SetAnimationCounts(size_t total_animations_count) {
  // Though these changes are pushed as part of AnimationHost::PushPropertiesTo
  // we don't SetNeedsPushProperties as pushing the values requires a commit.
  // Instead we allow them to be pushed whenever the next required commit
  // happens to avoid unnecessary work. See https://crbug.com/1083244.

  // If an animation is being run on the compositor, it will have a ticking
  // Animation (which will have a corresponding impl-thread version). Therefore
  // to find the count of main-only animations, we can simply subtract the
  // number of ticking animations from the total count.
  size_t ticking_animations_count = ticking_animations_.Read(*this).size();
  main_thread_animations_count_.Write(*this) =
      total_animations_count - ticking_animations_count;
  DCHECK_GE(main_thread_animations_count_.Read(*this), 0u);
}

size_t AnimationHost::MainThreadAnimationsCount() const {
  return main_thread_animations_count_.Read(*this);
}

bool AnimationHost::HasInvalidationAnimation() const {
  for (const auto& it : ticking_animations_.Read(*this))
    if (it->RequiresInvalidation())
      return true;
  return false;
}

bool AnimationHost::HasNativePropertyAnimation() const {
  for (const auto& it : ticking_animations_.Read(*this))
    if (it->AffectsNativeProperty())
      return true;
  return false;
}

AnimationHost::PendingThroughputTrackerInfos
AnimationHost::TakePendingThroughputTrackerInfos() {
  PendingThroughputTrackerInfos infos =
      std::move(pending_throughput_tracker_infos_.Write(*this));
  pending_throughput_tracker_infos_.Write(*this) = {};
  return infos;
}

void AnimationHost::StartThroughputTracking(
    TrackedAnimationSequenceId sequence_id) {
  pending_throughput_tracker_infos_.Write(*this).push_back({sequence_id, true});
  SetNeedsPushProperties();
}

void AnimationHost::StopThroughputTracking(
    TrackedAnimationSequenceId sequnece_id) {
  pending_throughput_tracker_infos_.Write(*this).push_back(
      {sequnece_id, false});
  SetNeedsPushProperties();
}

bool AnimationHost::HasScrollLinkedAnimation(ElementId for_scroller) const {
  for (auto& animation : ticking_animations_.Read(*this)) {
    if (auto* timeline = animation->animation_timeline()) {
      if (timeline->IsLinkedToScroller(for_scroller)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace cc
