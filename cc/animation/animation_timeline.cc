// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_timeline.h"

#include <algorithm>

#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/trees/property_tree.h"

namespace cc {

scoped_refptr<AnimationTimeline> AnimationTimeline::Create(int id) {
  return base::WrapRefCounted(new AnimationTimeline(id));
}

AnimationTimeline::AnimationTimeline(int id)
    : id_(id),
      animation_host_(),
      needs_push_properties_(false),
      is_impl_only_(false) {}

AnimationTimeline::~AnimationTimeline() {
  for (auto& kv : id_to_animation_map_)
    kv.second->SetAnimationTimeline(nullptr);
}

scoped_refptr<AnimationTimeline> AnimationTimeline::CreateImplInstance() const {
  scoped_refptr<AnimationTimeline> timeline = AnimationTimeline::Create(id());
  return timeline;
}

void AnimationTimeline::SetAnimationHost(AnimationHost* animation_host) {
  if (animation_host_ == animation_host)
    return;

  animation_host_ = animation_host;
  for (auto& kv : id_to_animation_map_)
    kv.second->SetAnimationHost(animation_host);

  SetNeedsPushProperties();
}

void AnimationTimeline::AttachAnimation(scoped_refptr<Animation> animation) {
  DCHECK(animation->id());
  animation->SetAnimationHost(animation_host_);
  animation->SetAnimationTimeline(this);
  id_to_animation_map_.insert(
      std::make_pair(animation->id(), std::move(animation)));

  SetNeedsPushProperties();
}

void AnimationTimeline::DetachAnimation(scoped_refptr<Animation> animation) {
  DCHECK(animation->id());
  EraseAnimation(animation);
  id_to_animation_map_.erase(animation->id());

  SetNeedsPushProperties();
}

Animation* AnimationTimeline::GetAnimationById(int animation_id) const {
  auto f = id_to_animation_map_.find(animation_id);
  return f == id_to_animation_map_.end() ? nullptr : f->second.get();
}

void AnimationTimeline::ClearAnimations() {
  for (auto& kv : id_to_animation_map_)
    EraseAnimation(kv.second);
  id_to_animation_map_.clear();

  SetNeedsPushProperties();
}

bool AnimationTimeline::TickTimeLinkedAnimations(
    const std::vector<scoped_refptr<Animation>>& ticking_animations,
    base::TimeTicks monotonic_time) {
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

    animation->Tick(monotonic_time);
    animated = true;
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
  needs_push_properties_ = true;
  if (animation_host_)
    animation_host_->SetNeedsPushProperties();
}

void AnimationTimeline::PushPropertiesTo(AnimationTimeline* timeline_impl) {
  if (needs_push_properties_) {
    needs_push_properties_ = false;
    PushAttachedAnimationsToImplThread(timeline_impl);
    RemoveDetachedAnimationsFromImplThread(timeline_impl);
    PushPropertiesToImplThread(timeline_impl);
  }
}

void AnimationTimeline::PushAttachedAnimationsToImplThread(
    AnimationTimeline* timeline_impl) const {
  for (auto& kv : id_to_animation_map_) {
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
  IdToAnimationMap& animations_impl = timeline_impl->id_to_animation_map_;

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
  animation->SetAnimationTimeline(nullptr);
  animation->SetAnimationHost(nullptr);
}

void AnimationTimeline::PushPropertiesToImplThread(
    AnimationTimeline* timeline_impl) {
  for (auto& kv : id_to_animation_map_) {
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

}  // namespace cc
