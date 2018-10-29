// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include <inttypes.h>
#include <algorithm>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/trees/property_animation_state.h"

namespace cc {

scoped_refptr<Animation> Animation::Create(int id) {
  return base::WrapRefCounted(new Animation(id));
}

Animation::Animation(int id)
    : animation_host_(),
      animation_timeline_(),
      animation_delegate_(),
      id_(id),
      ticking_keyframe_effects_count(0) {
  DCHECK(id_);
}

Animation::~Animation() {
  DCHECK(!animation_timeline_);
}

scoped_refptr<Animation> Animation::CreateImplInstance() const {
  return Animation::Create(id());
}

ElementId Animation::element_id_of_keyframe_effect(
    KeyframeEffectId keyframe_effect_id) const {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  return GetKeyframeEffectById(keyframe_effect_id)->element_id();
}

bool Animation::IsElementAttached(ElementId id) const {
  return base::ContainsKey(element_to_keyframe_effect_id_map_, id);
}

void Animation::SetAnimationHost(AnimationHost* animation_host) {
  animation_host_ = animation_host;
}

void Animation::SetAnimationTimeline(AnimationTimeline* timeline) {
  if (animation_timeline_ == timeline)
    return;

  // We need to unregister keyframe_effect to manage ElementAnimations and
  // observers properly.
  if (!element_to_keyframe_effect_id_map_.empty() && animation_host_) {
    // Destroy ElementAnimations or release it if it's still needed.
    UnregisterKeyframeEffects();
  }

  animation_timeline_ = timeline;

  // Register animation only if layer AND host attached. Unlike the
  // SingleKeyframeEffectAnimation case, all keyframe_effects have been attached
  // to their corresponding elements.
  if (!element_to_keyframe_effect_id_map_.empty() && animation_host_) {
    RegisterKeyframeEffects();
  }
}

bool Animation::has_element_animations() const {
  return !element_to_keyframe_effect_id_map_.empty();
}

scoped_refptr<ElementAnimations> Animation::element_animations(
    KeyframeEffectId keyframe_effect_id) const {
  return GetKeyframeEffectById(keyframe_effect_id)->element_animations();
}

void Animation::AttachElementForKeyframeEffect(
    ElementId element_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)->AttachElement(element_id);
  element_to_keyframe_effect_id_map_[element_id].emplace(keyframe_effect_id);
  // Register animation only if layer AND host attached.
  if (animation_host_) {
    // Create ElementAnimations or re-use existing.
    RegisterKeyframeEffect(element_id, keyframe_effect_id);
  }
}

void Animation::DetachElementForKeyframeEffect(
    ElementId element_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  DCHECK_EQ(GetKeyframeEffectById(keyframe_effect_id)->element_id(),
            element_id);

  UnregisterKeyframeEffect(element_id, keyframe_effect_id);
  GetKeyframeEffectById(keyframe_effect_id)->DetachElement();
  element_to_keyframe_effect_id_map_[element_id].erase(keyframe_effect_id);
}

void Animation::DetachElement() {
  if (animation_host_) {
    // Destroy ElementAnimations or release it if it's still needed.
    UnregisterKeyframeEffects();
  }

  for (auto pair = element_to_keyframe_effect_id_map_.begin();
       pair != element_to_keyframe_effect_id_map_.end();) {
    for (auto keyframe_effect = pair->second.begin();
         keyframe_effect != pair->second.end();) {
      GetKeyframeEffectById(*keyframe_effect)->DetachElement();
      keyframe_effect = pair->second.erase(keyframe_effect);
    }
    pair = element_to_keyframe_effect_id_map_.erase(pair);
  }
  DCHECK_EQ(element_to_keyframe_effect_id_map_.size(), 0u);
}

void Animation::RegisterKeyframeEffect(ElementId element_id,
                                       KeyframeEffectId keyframe_effect_id) {
  DCHECK(animation_host_);
  KeyframeEffect* keyframe_effect = GetKeyframeEffectById(keyframe_effect_id);
  DCHECK(!keyframe_effect->has_bound_element_animations());

  if (!keyframe_effect->has_attached_element())
    return;
  animation_host_->RegisterKeyframeEffectForElement(element_id,
                                                    keyframe_effect);
}

void Animation::UnregisterKeyframeEffect(ElementId element_id,
                                         KeyframeEffectId keyframe_effect_id) {
  DCHECK(animation_host_);
  KeyframeEffect* keyframe_effect = GetKeyframeEffectById(keyframe_effect_id);
  DCHECK(keyframe_effect);
  if (keyframe_effect->has_attached_element() &&
      keyframe_effect->has_bound_element_animations()) {
    animation_host_->UnregisterKeyframeEffectForElement(element_id,
                                                        keyframe_effect);
  }
}
void Animation::RegisterKeyframeEffects() {
  for (auto& element_id_keyframe_effect_id :
       element_to_keyframe_effect_id_map_) {
    const ElementId element_id = element_id_keyframe_effect_id.first;
    const std::unordered_set<KeyframeEffectId>& keyframe_effect_ids =
        element_id_keyframe_effect_id.second;
    for (auto& keyframe_effect_id : keyframe_effect_ids)
      RegisterKeyframeEffect(element_id, keyframe_effect_id);
  }
}

void Animation::UnregisterKeyframeEffects() {
  for (auto& element_id_keyframe_effect_id :
       element_to_keyframe_effect_id_map_) {
    const ElementId element_id = element_id_keyframe_effect_id.first;
    const std::unordered_set<KeyframeEffectId>& keyframe_effect_ids =
        element_id_keyframe_effect_id.second;
    for (auto& keyframe_effect_id : keyframe_effect_ids)
      UnregisterKeyframeEffect(element_id, keyframe_effect_id);
  }
  animation_host_->RemoveFromTicking(this);
}

void Animation::PushAttachedKeyframeEffectsToImplThread(
    Animation* animation_impl) const {
  for (auto& keyframe_effect : keyframe_effects_) {
    KeyframeEffect* keyframe_effect_impl =
        animation_impl->GetKeyframeEffectById(keyframe_effect->id());
    if (keyframe_effect_impl)
      continue;

    std::unique_ptr<KeyframeEffect> to_add =
        keyframe_effect->CreateImplInstance();
    animation_impl->AddKeyframeEffect(std::move(to_add));
  }
}

void Animation::PushPropertiesToImplThread(Animation* animation_impl) {
  for (auto& keyframe_effect : keyframe_effects_) {
    if (KeyframeEffect* keyframe_effect_impl =
            animation_impl->GetKeyframeEffectById(keyframe_effect->id())) {
      keyframe_effect->PushPropertiesTo(keyframe_effect_impl);
    }
  }
}

void Animation::AddKeyframeModelForKeyframeEffect(
    std::unique_ptr<KeyframeModel> keyframe_model,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->AddKeyframeModel(std::move(keyframe_model));
}

void Animation::PauseKeyframeModelForKeyframeEffect(
    int keyframe_model_id,
    double time_offset,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->PauseKeyframeModel(keyframe_model_id, time_offset);
}

void Animation::RemoveKeyframeModelForKeyframeEffect(
    int keyframe_model_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->RemoveKeyframeModel(keyframe_model_id);
}

void Animation::AbortKeyframeModelForKeyframeEffect(
    int keyframe_model_id,
    KeyframeEffectId keyframe_effect_id) {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  GetKeyframeEffectById(keyframe_effect_id)
      ->AbortKeyframeModel(keyframe_model_id);
}

void Animation::AbortKeyframeModelsWithProperty(
    TargetProperty::Type target_property,
    bool needs_completion) {
  for (auto& keyframe_effect : keyframe_effects_)
    keyframe_effect->AbortKeyframeModelsWithProperty(target_property,
                                                     needs_completion);
}

void Animation::PushPropertiesTo(Animation* animation_impl) {
  // In general when pushing proerties to impl thread we first push attached
  // properties to impl followed by removing the detached ones. However, we
  // never remove individual keyframe effect from an animation so there is no
  // need to remove the detached ones.
  PushAttachedKeyframeEffectsToImplThread(animation_impl);
  PushPropertiesToImplThread(animation_impl);
}

void Animation::Tick(base::TimeTicks monotonic_time) {
  DCHECK(!monotonic_time.is_null());
  for (auto& keyframe_effect : keyframe_effects_)
    keyframe_effect->Tick(monotonic_time);
}

void Animation::UpdateState(bool start_ready_animations,
                            AnimationEvents* events) {
  for (auto& keyframe_effect : keyframe_effects_) {
    keyframe_effect->UpdateState(start_ready_animations, events);
    keyframe_effect->UpdateTickingState(UpdateTickingType::NORMAL);
  }
}

void Animation::AddToTicking() {
  ++ticking_keyframe_effects_count;
  if (ticking_keyframe_effects_count > 1)
    return;
  DCHECK(animation_host_);
  animation_host_->AddToTicking(this);
}

void Animation::KeyframeModelRemovedFromTicking() {
  DCHECK_GE(ticking_keyframe_effects_count, 0);
  if (!ticking_keyframe_effects_count)
    return;
  --ticking_keyframe_effects_count;
  DCHECK(animation_host_);
  DCHECK_GE(ticking_keyframe_effects_count, 0);
  if (ticking_keyframe_effects_count)
    return;
  animation_host_->RemoveFromTicking(this);
}

void Animation::NotifyKeyframeModelStarted(const AnimationEvent& event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationStarted(
        event.monotonic_time, event.target_property, event.group_id);
  }
}

void Animation::NotifyKeyframeModelFinished(const AnimationEvent& event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationFinished(
        event.monotonic_time, event.target_property, event.group_id);
  }
}

void Animation::NotifyKeyframeModelAborted(const AnimationEvent& event) {
  if (animation_delegate_) {
    animation_delegate_->NotifyAnimationAborted(
        event.monotonic_time, event.target_property, event.group_id);
  }
}

void Animation::NotifyKeyframeModelTakeover(const AnimationEvent& event) {
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  if (animation_delegate_) {
    DCHECK(event.curve);
    std::unique_ptr<AnimationCurve> animation_curve = event.curve->Clone();
    animation_delegate_->NotifyAnimationTakeover(
        event.monotonic_time, event.target_property, event.animation_start_time,
        std::move(animation_curve));
  }
}

size_t Animation::TickingKeyframeModelsCount() const {
  size_t count = 0;
  for (auto& keyframe_effect : keyframe_effects_)
    count += keyframe_effect->TickingKeyframeModelsCount();
  return count;
}

void Animation::SetNeedsCommit() {
  DCHECK(animation_host_);
  animation_host_->SetNeedsCommit();
}

void Animation::SetNeedsPushProperties() {
  if (!animation_timeline_)
    return;
  animation_timeline_->SetNeedsPushProperties();
}

void Animation::ActivateKeyframeEffects() {
  for (auto& keyframe_effect : keyframe_effects_) {
    keyframe_effect->ActivateKeyframeEffects();
    keyframe_effect->UpdateTickingState(UpdateTickingType::NORMAL);
  }
}

KeyframeModel* Animation::GetKeyframeModelForKeyframeEffect(
    TargetProperty::Type target_property,
    KeyframeEffectId keyframe_effect_id) const {
  DCHECK(GetKeyframeEffectById(keyframe_effect_id));
  return GetKeyframeEffectById(keyframe_effect_id)
      ->GetKeyframeModel(target_property);
}

std::string Animation::ToString() const {
  std::string output = base::StringPrintf("Animation{id=%d", id_);
  for (const auto& keyframe_effect : keyframe_effects_) {
    output +=
        base::StringPrintf(", element_id=%s, keyframe_models=[%s]",
                           keyframe_effect->element_id().ToString().c_str(),
                           keyframe_effect->KeyframeModelsToString().c_str());
  }
  return output + "}";
}

bool Animation::IsWorkletAnimation() const {
  return false;
}

void Animation::AddKeyframeEffect(
    std::unique_ptr<KeyframeEffect> keyframe_effect) {
  keyframe_effect->SetAnimation(this);
  keyframe_effects_.push_back(std::move(keyframe_effect));

  SetNeedsPushProperties();
}

KeyframeEffect* Animation::GetKeyframeEffectById(
    KeyframeEffectId keyframe_effect_id) const {
  // May return nullptr when syncing keyframe_effects_ to impl.
  return keyframe_effects_.size() > keyframe_effect_id
             ? keyframe_effects_[keyframe_effect_id].get()
             : nullptr;
}

}  // namespace cc
