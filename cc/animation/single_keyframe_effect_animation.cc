// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/single_keyframe_effect_animation.h"

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

scoped_refptr<SingleKeyframeEffectAnimation>
SingleKeyframeEffectAnimation::Create(int id) {
  return base::WrapRefCounted(new SingleKeyframeEffectAnimation(id));
}

SingleKeyframeEffectAnimation::SingleKeyframeEffectAnimation(int id)
    : SingleKeyframeEffectAnimation(id, nullptr) {}

SingleKeyframeEffectAnimation::SingleKeyframeEffectAnimation(
    int id,
    size_t keyframe_effect_id)
    : SingleKeyframeEffectAnimation(
          id,
          std::make_unique<KeyframeEffect>(keyframe_effect_id)) {}

SingleKeyframeEffectAnimation::SingleKeyframeEffectAnimation(
    int id,
    std::unique_ptr<KeyframeEffect> keyframe_effect)
    : Animation(id) {
  DCHECK(id_);
  if (!keyframe_effect)
    keyframe_effect.reset(new KeyframeEffect(NextKeyframeEffectId()));

  AddKeyframeEffect(std::move(keyframe_effect));
}

SingleKeyframeEffectAnimation::~SingleKeyframeEffectAnimation() {}

KeyframeEffect* SingleKeyframeEffectAnimation::GetKeyframeEffect() const {
  DCHECK_EQ(keyframe_effects_.size(), 1u);
  return keyframe_effects_[0].get();
}

scoped_refptr<Animation> SingleKeyframeEffectAnimation::CreateImplInstance()
    const {
  DCHECK(GetKeyframeEffect());
  scoped_refptr<SingleKeyframeEffectAnimation> animation = base::WrapRefCounted(
      new SingleKeyframeEffectAnimation(id(), GetKeyframeEffect()->id()));
  return animation;
}

ElementId SingleKeyframeEffectAnimation::element_id() const {
  return GetKeyframeEffect()->element_id();
}

void SingleKeyframeEffectAnimation::AttachElement(ElementId element_id) {
  AttachElementForKeyframeEffect(element_id, GetKeyframeEffect()->id());
}

KeyframeEffect* SingleKeyframeEffectAnimation::keyframe_effect() const {
  return GetKeyframeEffect();
}

void SingleKeyframeEffectAnimation::AddKeyframeModel(
    std::unique_ptr<KeyframeModel> keyframe_model) {
  AddKeyframeModelForKeyframeEffect(std::move(keyframe_model),
                                    GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimation::PauseKeyframeModel(int keyframe_model_id,
                                                       double time_offset) {
  PauseKeyframeModelForKeyframeEffect(keyframe_model_id, time_offset,
                                      GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimation::RemoveKeyframeModel(int keyframe_model_id) {
  RemoveKeyframeModelForKeyframeEffect(keyframe_model_id,
                                       GetKeyframeEffect()->id());
}

void SingleKeyframeEffectAnimation::AbortKeyframeModel(int keyframe_model_id) {
  AbortKeyframeModelForKeyframeEffect(keyframe_model_id,
                                      GetKeyframeEffect()->id());
}

bool SingleKeyframeEffectAnimation::NotifyKeyframeModelFinishedForTesting(
    TargetProperty::Type target_property,
    int group_id) {
  AnimationEvent event(AnimationEvent::FINISHED,
                       GetKeyframeEffect()->element_id(), group_id,
                       target_property, base::TimeTicks());
  return GetKeyframeEffect()->NotifyKeyframeModelFinished(event);
}

KeyframeModel* SingleKeyframeEffectAnimation::GetKeyframeModel(
    TargetProperty::Type target_property) const {
  return GetKeyframeModelForKeyframeEffect(target_property,
                                           GetKeyframeEffect()->id());
}

}  // namespace cc
