// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_timeline.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/trees/property_tree.h"

namespace cc {

scoped_refptr<AnimationTimeline> AnimationTimeline::Create(int id,
                                                           bool impl_only) {
  return base::WrapRefCounted(new AnimationTimeline(id, impl_only));
}

AnimationTimeline::AnimationTimeline(int id, bool is_impl_only)
    : id_(id),
      animation_host_(),
      needs_push_properties_(false),
      is_impl_only_(is_impl_only) {}

AnimationTimeline::~AnimationTimeline() {
  for (auto& kv : id_to_animation_map_.Read(*this)) {
    kv.second->SetAnimationTimeline(nullptr);
  }
}

scoped_refptr<AnimationTimeline> AnimationTimeline::CreateImplInstance() const {
  scoped_refptr<AnimationTimeline> timeline = AnimationTimeline::Create(id());
  return timeline;
}

void AnimationTimeline::SetAnimationHost(AnimationHost* animation_host) {
  DCHECK(!animation_host || animation_host->IsOwnerThread());

  if (animation_host_ == animation_host)
    return;

  WaitForProtectedSequenceCompletion();

  animation_host_ = animation_host;
  for (auto& kv : id_to_animation_map_.Write(*this)) {
    kv.second->SetAnimationHost(animation_host);
  }

  SetNeedsPushProperties();
}

void AnimationTimeline::AttachAnimation(scoped_refptr<Animation> animation) {
  DCHECK(animation->id());
  animation->SetAnimationHost(animation_host());
  animation->SetAnimationTimeline(this);
  id_to_animation_map_.Write(*this).insert(
      std::make_pair(animation->id(), std::move(animation)));

  SetNeedsPushProperties();
}

void AnimationTimeline::DetachAnimation(scoped_refptr<Animation> animation) {
  DCHECK(animation->id());
  EraseAnimation(animation);
  id_to_animation_map_.Write(*this).erase(animation->id());

  SetNeedsPushProperties();
}

Animation* AnimationTimeline::GetAnimationById(int animation_id) const {
  auto f = id_to_animation_map_.Read(*this).find(animation_id);
  return f == id_to_animation_map_.Read(*this).end() ? nullptr
                                                     : f->second.get();
}

void AnimationTimeline::ClearAnimations() {
  for (auto& kv : id_to_animation_map_.Write(*this)) {
    EraseAnimation(kv.second);
  }
  id_to_animation_map_.Write(*this).clear();

  SetNeedsPushProperties();
}

bool AnimationTimeline::TickTimeLinkedAnimations(
    const std::vector<scoped_refptr<Animation>>& ticking_animations,
    base::TimeTicks monotonic_time,
    bool tick_finished) {
  DCHECK(!IsScrollTimeline());

  bool animated = false;
  for (auto& animation : ticking_animations) {
    if (animation->animation_timeline() != this)
      continue;
    // Worklet animations are ticked separately by AnimationHost.
    if (animation->IsWorkletAnimation())
      continue;

    // Scroll-linked animations are ticked separately.
    if (animation->IsScrollLinkedAnimation())
      continue;

    if (!tick_finished && animation->keyframe_effect()->awaiting_deletion()) {
      continue;
    }

    animated |= animation->Tick(monotonic_time);
  }
  return animated;
}

bool AnimationTimeline::TickScrollLinkedAnimations(
    const std::vector<scoped_refptr<Animation>>& ticking_animations,
    const ScrollTree& scroll_tree,
    bool is_active_tree) {
  return false;
}

void AnimationTimeline::SetNeedsPushProperties() {
  needs_push_properties_.Write(*this) = true;
  if (animation_host()) {
    animation_host()->SetNeedsPushProperties();
  }
}

void AnimationTimeline::PushPropertiesTo(AnimationTimeline* timeline_impl) {
  if (needs_push_properties_.Read(*this)) {
    needs_push_properties_.Write(*this) = false;
    PushAttachedAnimationsToImplThread(timeline_impl);
    RemoveDetachedAnimationsFromImplThread(timeline_impl);
    PushPropertiesToImplThread(timeline_impl);
  }
}

void AnimationTimeline::PushAttachedAnimationsToImplThread(
    AnimationTimeline* timeline_impl) const {
  for (auto& kv : id_to_animation_map_.Read(*this)) {
    auto& animation = kv.second;
    Animation* animation_impl =
        timeline_impl->GetAnimationById(animation->id());
    if (animation_impl)
      continue;

    scoped_refptr<Animation> to_add = animation->CreateImplInstance();
    timeline_impl->AttachAnimation(to_add.get());
  }
}

void AnimationTimeline::RemoveDetachedAnimationsFromImplThread(
    AnimationTimeline* timeline_impl) const {
  IdToAnimationMap& animations_impl =
      timeline_impl->id_to_animation_map_.Write(*this);

  // Erase all the impl animations which |this| doesn't have.
  for (auto it = animations_impl.begin(); it != animations_impl.end();) {
    if (GetAnimationById(it->second->id())) {
      ++it;
    } else {
      timeline_impl->EraseAnimation(it->second);
      it = animations_impl.erase(it);
    }
  }
}

void AnimationTimeline::EraseAnimation(scoped_refptr<Animation> animation) {
  if (animation->element_animations())
    animation->DetachElement();
  animation->SetAnimationHost(nullptr);
  animation->SetAnimationTimeline(nullptr);
}

void AnimationTimeline::PushPropertiesToImplThread(
    AnimationTimeline* timeline_impl) {
  for (auto& kv : id_to_animation_map_.Read(*this)) {
    Animation* animation = kv.second.get();
    if (Animation* animation_impl =
            timeline_impl->GetAnimationById(animation->id())) {
      animation->PushPropertiesTo(animation_impl);
    }
  }
}

bool AnimationTimeline::IsScrollTimeline() const {
  return false;
}

bool AnimationTimeline::IsLinkedToScroller(ElementId scroller) const {
  return false;
}

bool AnimationTimeline::IsOwnerThread() const {
  return !animation_host_ || animation_host_->IsOwnerThread();
}

bool AnimationTimeline::InProtectedSequence() const {
  return !animation_host_ || animation_host_->InProtectedSequence();
}

void AnimationTimeline::WaitForProtectedSequenceCompletion() const {
  if (animation_host_) {
    animation_host_->WaitForProtectedSequenceCompletion();
  }
}

}  // namespace cc
