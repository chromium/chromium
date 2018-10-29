// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include <stddef.h>

#include <algorithm>

#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/transform_operations.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

scoped_refptr<ElementAnimations> ElementAnimations::Create() {
  return base::WrapRefCounted(new ElementAnimations());
}

ElementAnimations::ElementAnimations()
    : animation_host_(),
      element_id_(),
      has_element_in_active_list_(false),
      has_element_in_pending_list_(false),
      needs_push_properties_(false) {}

ElementAnimations::~ElementAnimations() = default;

void ElementAnimations::SetAnimationHost(AnimationHost* host) {
  animation_host_ = host;
}

void ElementAnimations::SetElementId(ElementId element_id) {
  element_id_ = element_id;
}

void ElementAnimations::InitAffectedElementTypes() {
  DCHECK(element_id_);
  DCHECK(animation_host_);

  UpdateKeyframeEffectsTickingState(UpdateTickingType::FORCE);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsElementInList(
          element_id_, ElementListType::ACTIVE)) {
    set_has_element_in_active_list(true);
  }
  if (animation_host_->mutator_host_client()->IsElementInList(
          element_id_, ElementListType::PENDING)) {
    set_has_element_in_pending_list(true);
  }
}

TargetProperties ElementAnimations::GetPropertiesMaskForAnimationState() {
  TargetProperties properties;
  properties[TargetProperty::TRANSFORM] = true;
  properties[TargetProperty::OPACITY] = true;
  properties[TargetProperty::FILTER] = true;
  return properties;
}

void ElementAnimations::ClearAffectedElementTypes() {
  DCHECK(animation_host_);

  TargetProperties disable_properties = GetPropertiesMaskForAnimationState();
  PropertyAnimationState disabled_state_mask, disabled_state;
  disabled_state_mask.currently_running = disable_properties;
  disabled_state_mask.potentially_animating = disable_properties;

  // This method may get called from AnimationHost dtor so it is possible for
  // mutator_host_client() to be null.
  if (has_element_in_active_list() && animation_host()->mutator_host_client()) {
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::ACTIVE, disabled_state_mask,
        disabled_state);
  }
  set_has_element_in_active_list(false);

  if (has_element_in_pending_list() &&
      animation_host()->mutator_host_client()) {
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::PENDING, disabled_state_mask,
        disabled_state);
  }
  set_has_element_in_pending_list(false);

  RemoveKeyframeEffectsFromTicking();
}

void ElementAnimations::ElementRegistered(ElementId element_id,
                                          ElementListType list_type) {
  DCHECK_EQ(element_id_, element_id);

  if (!has_element_in_any_list())
    UpdateKeyframeEffectsTickingState(UpdateTickingType::FORCE);

  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(true);
  else
    set_has_element_in_pending_list(true);
}

void ElementAnimations::ElementUnregistered(ElementId element_id,
                                            ElementListType list_type) {
  DCHECK_EQ(this->element_id(), element_id);
  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(false);
  else
    set_has_element_in_pending_list(false);

  if (!has_element_in_any_list())
    RemoveKeyframeEffectsFromTicking();
}

void ElementAnimations::AddKeyframeEffect(KeyframeEffect* keyframe_effect) {
  keyframe_effects_list_.AddObserver(keyframe_effect);
  keyframe_effect->BindElementAnimations(this);
}

void ElementAnimations::RemoveKeyframeEffect(KeyframeEffect* keyframe_effect) {
  keyframe_effects_list_.RemoveObserver(keyframe_effect);
  keyframe_effect->UnbindElementAnimations();
}

bool ElementAnimations::IsEmpty() const {
  return !keyframe_effects_list_.might_have_observers();
}

void ElementAnimations::SetNeedsPushProperties() {
  needs_push_properties_ = true;
}

void ElementAnimations::PushPropertiesTo(
    scoped_refptr<ElementAnimations> element_animations_impl) const {
  DCHECK_NE(this, element_animations_impl);

  if (!needs_push_properties_)
    return;
  needs_push_properties_ = false;

  element_animations_impl->UpdateClientAnimationState();
}

void ElementAnimations::UpdateKeyframeEffectsTickingState(
    UpdateTickingType update_ticking_type) const {
  for (auto& keyframe_effect : keyframe_effects_list_)
    keyframe_effect.UpdateTickingState(update_ticking_type);
}

void ElementAnimations::RemoveKeyframeEffectsFromTicking() const {
  for (auto& keyframe_effect : keyframe_effects_list_)
    keyframe_effect.RemoveFromTicking();
}

void ElementAnimations::NotifyAnimationStarted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.NotifyKeyframeModelStarted(event))
      break;
  }
}

void ElementAnimations::NotifyAnimationFinished(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.NotifyKeyframeModelFinished(event))
      break;
  }
}

void ElementAnimations::NotifyAnimationTakeover(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);
  DCHECK(event.target_property == TargetProperty::SCROLL_OFFSET);

  for (auto& keyframe_effect : keyframe_effects_list_)
    keyframe_effect.NotifyKeyframeModelTakeover(event);
}

void ElementAnimations::NotifyAnimationAborted(const AnimationEvent& event) {
  DCHECK(!event.is_impl_only);

  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.NotifyKeyframeModelAborted(event))
      break;
  }

  UpdateClientAnimationState();
}

bool ElementAnimations::HasOnlyTranslationTransforms(
    ElementListType list_type) const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (!keyframe_effect.HasOnlyTranslationTransforms(list_type))
      return false;
  }
  return true;
}

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (!keyframe_effect.AnimationsPreserveAxisAlignment())
      return false;
  }
  return true;
}

bool ElementAnimations::AnimationStartScale(ElementListType list_type,
                                            float* start_scale) const {
  *start_scale = 0.f;

  for (auto& keyframe_effect : keyframe_effects_list_) {
    float keyframe_effect_start_scale = 0.f;
    bool success = keyframe_effect.AnimationStartScale(
        list_type, &keyframe_effect_start_scale);
    if (!success)
      return false;
    // Union: a maximum.
    *start_scale = std::max(*start_scale, keyframe_effect_start_scale);
  }

  return true;
}

bool ElementAnimations::MaximumTargetScale(ElementListType list_type,
                                           float* max_scale) const {
  *max_scale = 0.f;

  for (auto& keyframe_effect : keyframe_effects_list_) {
    float keyframe_effect_max_scale = 0.f;
    bool success = keyframe_effect.MaximumTargetScale(
        list_type, &keyframe_effect_max_scale);
    if (!success)
      return false;
    // Union: a maximum.
    *max_scale = std::max(*max_scale, keyframe_effect_max_scale);
  }

  return true;
}

bool ElementAnimations::ScrollOffsetAnimationWasInterrupted() const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.scroll_offset_animation_was_interrupted())
      return true;
  }
  return false;
}

void ElementAnimations::NotifyClientFloatAnimated(
    float opacity,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  DCHECK(keyframe_model->target_property_id() == TargetProperty::OPACITY);
  opacity = base::ClampToRange(opacity, 0.0f, 1.0f);
  if (KeyframeModelAffectsActiveElements(keyframe_model))
    OnOpacityAnimated(ElementListType::ACTIVE, opacity);
  if (KeyframeModelAffectsPendingElements(keyframe_model))
    OnOpacityAnimated(ElementListType::PENDING, opacity);
}

void ElementAnimations::NotifyClientFilterAnimated(
    const FilterOperations& filters,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  if (KeyframeModelAffectsActiveElements(keyframe_model))
    OnFilterAnimated(ElementListType::ACTIVE, filters);
  if (KeyframeModelAffectsPendingElements(keyframe_model))
    OnFilterAnimated(ElementListType::PENDING, filters);
}

void ElementAnimations::NotifyClientTransformOperationsAnimated(
    const TransformOperations& operations,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  gfx::Transform transform = operations.Apply();
  if (KeyframeModelAffectsActiveElements(keyframe_model))
    OnTransformAnimated(ElementListType::ACTIVE, transform);
  if (KeyframeModelAffectsPendingElements(keyframe_model))
    OnTransformAnimated(ElementListType::PENDING, transform);
}

void ElementAnimations::NotifyClientScrollOffsetAnimated(
    const gfx::ScrollOffset& scroll_offset,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  if (KeyframeModelAffectsActiveElements(keyframe_model))
    OnScrollOffsetAnimated(ElementListType::ACTIVE, scroll_offset);
  if (KeyframeModelAffectsPendingElements(keyframe_model))
    OnScrollOffsetAnimated(ElementListType::PENDING, scroll_offset);
}

void ElementAnimations::UpdateClientAnimationState() {
  if (!element_id())
    return;
  DCHECK(animation_host());
  if (!animation_host()->mutator_host_client())
    return;

  PropertyAnimationState prev_pending = pending_state_;
  PropertyAnimationState prev_active = active_state_;

  pending_state_.Clear();
  active_state_.Clear();

  for (auto& keyframe_effect : keyframe_effects_list_) {
    PropertyAnimationState keyframe_effect_pending_state,
        keyframe_effect_active_state;
    keyframe_effect.GetPropertyAnimationState(&keyframe_effect_pending_state,
                                              &keyframe_effect_active_state);
    pending_state_ |= keyframe_effect_pending_state;
    active_state_ |= keyframe_effect_active_state;
  }

  TargetProperties allowed_properties = GetPropertiesMaskForAnimationState();
  PropertyAnimationState allowed_state;
  allowed_state.currently_running = allowed_properties;
  allowed_state.potentially_animating = allowed_properties;

  pending_state_ &= allowed_state;
  active_state_ &= allowed_state;

  DCHECK(pending_state_.IsValid());
  DCHECK(active_state_.IsValid());

  if (has_element_in_active_list() && prev_active != active_state_) {
    PropertyAnimationState diff_active = prev_active ^ active_state_;
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::ACTIVE, diff_active, active_state_);
  }
  if (has_element_in_pending_list() && prev_pending != pending_state_) {
    PropertyAnimationState diff_pending = prev_pending ^ pending_state_;
    animation_host()->mutator_host_client()->ElementIsAnimatingChanged(
        element_id(), ElementListType::PENDING, diff_pending, pending_state_);
  }
}

bool ElementAnimations::HasTickingKeyframeEffect() const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.HasTickingKeyframeModel())
      return true;
  }

  return false;
}

bool ElementAnimations::HasAnyKeyframeModel() const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.has_any_keyframe_model())
      return true;
  }

  return false;
}

bool ElementAnimations::HasAnyAnimationTargetingProperty(
    TargetProperty::Type property) const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.GetKeyframeModel(property))
      return true;
  }
  return false;
}

bool ElementAnimations::IsPotentiallyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.IsPotentiallyAnimatingProperty(target_property,
                                                       list_type))
      return true;
  }

  return false;
}

bool ElementAnimations::IsCurrentlyAnimatingProperty(
    TargetProperty::Type target_property,
    ElementListType list_type) const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.IsCurrentlyAnimatingProperty(target_property,
                                                     list_type))
      return true;
  }

  return false;
}

void ElementAnimations::OnFilterAnimated(ElementListType list_type,
                                         const FilterOperations& filters) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementFilterMutated(
      element_id(), list_type, filters);
}

void ElementAnimations::OnOpacityAnimated(ElementListType list_type,
                                          float opacity) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementOpacityMutated(
      element_id(), list_type, opacity);
}

void ElementAnimations::OnTransformAnimated(ElementListType list_type,
                                            const gfx::Transform& transform) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementTransformMutated(
      element_id(), list_type, transform);
}

void ElementAnimations::OnScrollOffsetAnimated(
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(element_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetElementScrollOffsetMutated(
      element_id(), list_type, scroll_offset);
}

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  if (animation_host()) {
    DCHECK(animation_host()->mutator_host_client());
    return animation_host()->mutator_host_client()->GetScrollOffsetForAnimation(
        element_id());
  }

  return gfx::ScrollOffset();
}

bool ElementAnimations::KeyframeModelAffectsActiveElements(
    KeyframeModel* keyframe_model) const {
  // When we force a keyframe_model update due to a notification, we do not have
  // a KeyframeModel instance. In this case, we force an update of active
  // elements.
  if (!keyframe_model)
    return true;
  return keyframe_model->affects_active_elements() &&
         has_element_in_active_list();
}

bool ElementAnimations::KeyframeModelAffectsPendingElements(
    KeyframeModel* keyframe_model) const {
  // When we force a keyframe_model update due to a notification, we do not have
  // a KeyframeModel instance. In this case, we force an update of pending
  // elements.
  if (!keyframe_model)
    return true;
  return keyframe_model->affects_pending_elements() &&
         has_element_in_pending_list();
}

}  // namespace cc
