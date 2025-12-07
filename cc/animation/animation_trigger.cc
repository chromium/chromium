// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_trigger.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"

namespace cc {

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

void AnimationTrigger::SetNeedsPushProperties() {
  if (animation_host_) {
    animation_host_->SetNeedsPushProperties();
  }
}

}  // namespace cc
