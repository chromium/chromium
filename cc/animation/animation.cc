// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include <inttypes.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/stringprintf.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/trees/property_animation_state.h"

namespace cc {

scoped_refptr<Animation> Animation::Create(int id) {
  return base::WrapRefCounted(new Animation(id));
}

Animation::Animation(int id) : id_(id) {
  DCHECK(id_);
  keyframe_effect_.Write(*this) = std::make_unique<KeyframeEffect>(this);
}

Animation::~Animation() {
  DCHECK(!animation_timeline());
}

scoped_refptr<Animation> Animation::CreateImplInstance() const {
  return Animation::Create(id());
}

ElementId Animation::element_id() const {
  return keyframe_effect()->element_id();
}

void Animation::SetAnimationHost(AnimationHost* animation_host) {
  DCHECK(IsOwnerThread());
  if (animation_host == animation_host_)
    return;

  WaitForProtectedSequenceCompletion();

  animation_host_ = animation_host;
}

void Animation::SetAnimationTimeline(AnimationTimeline* timeline) {
  if (animation_timeline() == timeline)
    return;

  // We need to unregister the animation to manage ElementAnimations and
  // observers properly.
  if (keyframe_effect()->has_attached_element() &&
      keyframe_effect()->has_bound_element_animations()) {
    UnregisterAnimation();
  }

  animation_timeline_.Write(*this) = timeline;

  // Register animation only if layer AND host attached.
  if (keyframe_effect()->has_attached_element() && animation_host())
    RegisterAnimation();
}

scoped_refptr<const ElementAnimations> Animation::element_animations() const {
  return keyframe_effect()->element_animations();
}

void Animation::AttachElement(ElementId element_id) {
  DCHECK_NE(element_id, kReservedElementIdForPaintWorklet);
  AttachElementInternal(element_id);
}

void Animation::AttachPaintWorkletElement() {
  AttachElementInternal(kReservedElementIdForPaintWorklet);
}

void Animation::AttachElementInternal(ElementId element_id) {
  keyframe_effect()->AttachElement(element_id);
  // Register animation only if layer AND host attached.
  if (animation_host())
    RegisterAnimation();
}

void Animation::SetKeyframeEffectForTesting(
    std::unique_ptr<KeyframeEffect> effect) {
  keyframe_effect_.Write(*this) = std::move(effect);
}

bool Animation::IsOwnerThread() const {
  return !animation_host_ || animation_host_->IsOwnerThread();
}

bool Animation::InProtectedSequence() const {
  return !animation_host_ || animation_host_->InProtectedSequence();
}

void Animation::WaitForProtectedSequenceCompletion() const {
  if (animation_host_)
    animation_host_->WaitForProtectedSequenceCompletion();
}

void Animation::DetachElement() {
  DCHECK(keyframe_effect()->has_attached_element());

  if (animation_host())
    UnregisterAnimation();

  keyframe_effect()->DetachElement();
}

void Animation::RegisterAnimation() {
  DCHECK(animation_host());
  DCHECK(keyframe_effect()->has_attached_element());
  DCHECK(!keyframe_effect()->has_bound_element_animations());

  // Create ElementAnimations or re-use existing.
  animation_host()->RegisterAnimationForElement(keyframe_effect()->element_id(),
                                                this);
}

void Animation::UnregisterAnimation() {
  DCHECK(animation_host());
  DCHECK(keyframe_effect()->has_attached_element());
  DCHECK(keyframe_effect()->has_bound_element_animations());

  // Destroy ElementAnimations or release it if it's still needed.
  animation_host()->UnregisterAnimationForElement(
      keyframe_effect()->element_id(), this);
}

void Animation::PushPropertiesTo(Animation* animation_impl) {
  std::optional<base::TimeTicks> impl_start_time;
  if (is_replacement_ && !keyframe_effect()->keyframe_models().empty()) {
    auto* cc_keyframe_model = KeyframeModel::ToCcKeyframeModel(
        keyframe_effect()->keyframe_models().front().get());
    animation_impl->keyframe_effect()->set_replaced_group(
        cc_keyframe_model->group());
  }

  if (is_replacement_ && !GetStartTime()) {
    // If this animation is replacing an existing one before having received a
    // start time, try to get the start from the animation being replaced.
    // This is done to prevent a race where the client may cancel and restart
    // the Animation before having received a start time but after the
    // Animation has started playing on the compositor thread.
    impl_start_time = animation_impl->GetStartTime();

    // This should always happen only on the first commit which must need
    // pushing (and hence, the below call won't no-op).
    CHECK(keyframe_effect()->needs_push_properties());
  }
  is_replacement_ = false;

  keyframe_effect()->PushPropertiesTo(animation_impl->keyframe_effect(),
                                      impl_start_time);
}

bool Animation::Tick(base::TimeTicks tick_time) {
  DCHECK(!IsWorkletAnimation());
  return keyframe_effect()->Tick(tick_time);
}

bool Animation::IsScrollLinkedAnimation() const {
  return animation_timeline() && animation_timeline()->IsScrollTimeline();
}

void Animation::UpdateState(bool start_ready_animations,
                            AnimationEvents* events) {
  keyframe_effect()->UpdateState(start_ready_animations, events);
  keyframe_effect()->UpdateTickingState();
}

void Animation::AddToTicking() {
  DCHECK(animation_host());
  animation_host()->AddToTicking(this);
}

void Animation::RemoveFromTicking() {
  DCHECK(animation_host());
  animation_host()->RemoveFromTicking(this);
}

void Animation::DispatchAndDelegateAnimationEvent(const AnimationEvent& event) {
  if (event.ShouldDispatchToKeyframeEffectAndModel()) {
    if (!keyframe_effect() ||
        !keyframe_effect()->DispatchAnimationEventToKeyframeModel(event)) {
      // If we fail to dispatch the event, it is to clean up an obsolete
      // animation and should not notify the delegate.
      // TODO(gerchiko): Determine when we expect the referenced animations not
      // to exist.
      return;
    }
  }
  DelegateAnimationEvent(event);
}

void Animation::DelegateAnimationEvent(const AnimationEvent& event) {
  if (animation_delegate_) {
    switch (event.type) {
      case AnimationEvent::Type::kStarted:
        animation_delegate_->NotifyAnimationStarted(
            event.monotonic_time, event.target_property, event.group_id);
        break;

      case AnimationEvent::Type::kFinished:
        animation_delegate_->NotifyAnimationFinished(
            event.monotonic_time, event.target_property, event.group_id);
        break;

      case AnimationEvent::Type::kAborted:
        animation_delegate_->NotifyAnimationAborted(
            event.monotonic_time, event.target_property, event.group_id);
        break;

      case AnimationEvent::Type::kTakeOver:
        // TODO(crbug.com/40655283): Routing TAKEOVER events is broken.
        DCHECK(!event.is_impl_only);
        DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);
        DCHECK(event.curve);
        animation_delegate_->NotifyAnimationTakeover(
            event.monotonic_time, event.target_property,
            event.animation_start_time, event.curve->Clone());
        break;

      case AnimationEvent::Type::kTimeUpdated:
        DCHECK(!event.is_impl_only);
        animation_delegate_->NotifyLocalTimeUpdated(event.local_time);
        break;
    }
  }
}

bool Animation::RequiresInvalidation() const {
  return keyframe_effect()->RequiresInvalidation();
}

bool Animation::AffectsNativeProperty() const {
  return keyframe_effect()->AffectsNativeProperty();
}

void Animation::SetNeedsCommit() {
  DCHECK(animation_host());
  animation_host()->SetNeedsCommit();
}

std::optional<base::TimeTicks> Animation::GetStartTime() const {
  CHECK(keyframe_effect());

  if (!keyframe_effect()->keyframe_models().size()) {
    return std::nullopt;
  }

  // KeyframeModels should all share the same start time so just use the first
  // one's.
  gfx::KeyframeModel& km = *keyframe_effect()->keyframe_models().front();

  if (!km.has_set_start_time()) {
    return std::nullopt;
  }

  return km.start_time();
}

void Animation::SetNeedsPushProperties() {
  if (!animation_timeline())
    return;
  animation_timeline()->SetNeedsPushProperties();
}

void Animation::ActivateKeyframeModels() {
  keyframe_effect()->ActivateKeyframeModels();
  keyframe_effect()->UpdateTickingState();
}

KeyframeModel* Animation::GetKeyframeModel(
    TargetProperty::Type target_property) const {
  return KeyframeModel::ToCcKeyframeModel(
      keyframe_effect()->GetKeyframeModel(target_property));
}

std::string Animation::ToString() const {
  return base::StringPrintf(
      "Animation{id=%d, element_id=%s, keyframe_models=[%s]}", id_,
      keyframe_effect()->element_id().ToString().c_str(),
      keyframe_effect()->KeyframeModelsToString().c_str());
}

bool Animation::IsWorkletAnimation() const {
  return false;
}

void Animation::AddKeyframeModel(
    std::unique_ptr<KeyframeModel> keyframe_model) {
  keyframe_effect()->AddKeyframeModel(std::move(keyframe_model));
}

void Animation::PauseKeyframeModel(int keyframe_model_id,
                                   base::TimeDelta time_offset) {
  keyframe_effect()->PauseKeyframeModel(keyframe_model_id, time_offset);
}

void Animation::RemoveKeyframeModel(int keyframe_model_id) {
  keyframe_effect()->RemoveKeyframeModel(keyframe_model_id);
}

void Animation::AbortKeyframeModel(int keyframe_model_id) {
  keyframe_effect()->AbortKeyframeModel(keyframe_model_id);
}

void Animation::AbortKeyframeModelsWithProperty(
    TargetProperty::Type target_property,
    bool needs_completion) {
  keyframe_effect()->AbortKeyframeModelsWithProperty(target_property,
                                                     needs_completion);
}

void Animation::NotifyKeyframeModelFinishedForTesting(
    int timeline_id,
    int keyframe_model_id,
    TargetProperty::Type target_property,
    int group_id) {
  AnimationEvent event(AnimationEvent::Type::kFinished,
                       {timeline_id, id(), keyframe_model_id}, group_id,
                       target_property, base::TimeTicks());
  DispatchAndDelegateAnimationEvent(event);
}

}  // namespace cc
