// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_trigger.h"

#include <vector>

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/animation_trigger_delegate.h"
#include "cc/animation/keyframe_effect.h"

namespace cc {

AnimationTrigger::AnimationData::AnimationData(int animation_id,
                                               int timeline_id,
                                               Behavior activate_behavior,
                                               Behavior deactivate_behavior)
    : animation_id(animation_id),
      timeline_id(timeline_id),
      activate_behavior(activate_behavior),
      deactivate_behavior(deactivate_behavior) {}

AnimationTrigger::AnimationData::AnimationData(const AnimationData& data) =
    default;

bool AnimationTrigger::AnimationData::operator==(
    const AnimationData& other) const = default;

AnimationTrigger::AnimationData::~AnimationData() = default;

AnimationTrigger::AnimationTrigger(int id) : id_(id) {}

AnimationTrigger::~AnimationTrigger() = default;

bool AnimationTrigger::IsEventTrigger() const {
  return false;
}

bool AnimationTrigger::IsTimelineTrigger() const {
  return false;
}

bool AnimationTrigger::IsOwnerThread() const {
  return !animation_host_ || animation_host_->IsOwnerThread();
}

bool AnimationTrigger::InProtectedSequence() const {
  return !animation_host_ || animation_host_->InProtectedSequence();
}

void AnimationTrigger::WaitForProtectedSequenceCompletion() const {
  if (animation_host_) {
    animation_host_->WaitForProtectedSequenceCompletion();
  }
}

void AnimationTrigger::PushPropertiesTo(AnimationTrigger* trigger_impl) {
  trigger_impl->animation_data_.Write(*trigger_impl) =
      animation_data_.Read(*this);
}

void AnimationTrigger::SetNeedsPushProperties() {
  if (animation_host_) {
    animation_host_->SetNeedsPushProperties();
  }
}

const std::vector<AnimationTrigger::AnimationData>&
AnimationTrigger::GetAnimationDataForTest() {
  return animation_data_.Read(*this);
}

void AnimationTrigger::SetAnimationData(std::vector<AnimationData>& data) {
  if (animation_data_.Read(*this) != data) {
    SetNeedsPushProperties();
    animation_data_.Write(*this) = data;
  }
}

void AnimationTrigger::PerformActivate(AnimationEvents* events) {
  DCHECK(events);
  events->events().emplace_back(
      AnimationTriggerEvent(id(), AnimationTriggerEvent::Type::kActivate));
  // TODO(crbug.com/451238244): Trigger animations.
}

void AnimationTrigger::PerformDeactivate(AnimationEvents* events) {
  DCHECK(events);
  events->events().emplace_back(
      AnimationTriggerEvent(id(), AnimationTriggerEvent::Type::kDeactivate));
  // TODO(crbug.com/451238244): Trigger animations.
}

void AnimationTrigger::SetAnimationTriggerDelegate(
    AnimationTriggerDelegate* delegate) {
  animation_trigger_delegate_ = delegate;
}

void AnimationTrigger::DispatchAnimationTriggerEvent(
    const AnimationTriggerEvent& event) {
  if (animation_trigger_delegate_) {
    switch (event.type) {
      case AnimationTriggerEvent::Type::kActivate:
        animation_trigger_delegate_->NotifyActivated();
        break;
      case AnimationTriggerEvent::Type::kDeactivate:
        animation_trigger_delegate_->NotifyDeactivated();
    }
  }
}

}  // namespace cc
