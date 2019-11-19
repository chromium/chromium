// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include <stddef.h>

#include <algorithm>

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

namespace {

// After BlinkGenPropertyTrees, the targeted ElementId depends on the property
// being mutated. If an ElementId is set on the KeyframeModel, we should apply
// the mutation to the specific element.
// TODO(flackr): Remove ElementId from ElementAnimations once all element
// tracking is done on the KeyframeModel - https://crbug.com/900241
ElementId CalculateTargetElementId(const ElementAnimations* element_animations,
                                   const KeyframeModel* keyframe_model) {
  if (LIKELY(keyframe_model->element_id()))
    return keyframe_model->element_id();
  return element_animations->element_id();
}

}  // namespace

scoped_refptr<ElementAnimations> ElementAnimations::Create(
    AnimationHost* host,
    ElementId element_id) {
  DCHECK(element_id);
  DCHECK(host);
  return base::WrapRefCounted(new ElementAnimations(host, element_id));
}

ElementAnimations::ElementAnimations(AnimationHost* host, ElementId element_id)
    : animation_host_(host),
      element_id_(element_id),
      has_element_in_active_list_(false),
      has_element_in_pending_list_(false),
      needs_push_properties_(false),
      active_maximum_scale_(kNotScaled),
      active_starting_scale_(kNotScaled),
      pending_maximum_scale_(kNotScaled),
      pending_starting_scale_(kNotScaled) {
  InitAffectedElementTypes();
}

ElementAnimations::~ElementAnimations() = default;

void ElementAnimations::InitAffectedElementTypes() {
  DCHECK(element_id_);
  DCHECK(animation_host_);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsElementInPropertyTrees(
          element_id_, ElementListType::ACTIVE)) {
    set_has_element_in_active_list(true);
  }
  if (animation_host_->mutator_host_client()->IsElementInPropertyTrees(
          element_id_, ElementListType::PENDING)) {
    set_has_element_in_pending_list(true);
  }
}

TargetProperties ElementAnimations::GetPropertiesMaskForAnimationState() {
  TargetProperties properties;
  properties[TargetProperty::TRANSFORM] = true;
  properties[TargetProperty::OPACITY] = true;
  properties[TargetProperty::FILTER] = true;
  properties[TargetProperty::BACKDROP_FILTER] = true;
  return properties;
}

void ElementAnimations::ClearAffectedElementTypes(
    const PropertyToElementIdMap& element_id_map) {
  DCHECK(animation_host_);

  TargetProperties disable_properties = GetPropertiesMaskForAnimationState();
  PropertyAnimationState disabled_state_mask, disabled_state;
  disabled_state_mask.currently_running = disable_properties;
  disabled_state_mask.potentially_animating = disable_properties;

  // This method may get called from AnimationHost dtor so it is possible for
  // mutator_host_client() to be null.
  if (has_element_in_active_list() && animation_host_->mutator_host_client()) {
    animation_host_->mutator_host_client()->ElementIsAnimatingChanged(
        element_id_map, ElementListType::ACTIVE, disabled_state_mask,
        disabled_state);
  }
  set_has_element_in_active_list(false);

  if (has_element_in_pending_list() && animation_host_->mutator_host_client()) {
    animation_host_->mutator_host_client()->ElementIsAnimatingChanged(
        element_id_map, ElementListType::PENDING, disabled_state_mask,
        disabled_state);
  }
  set_has_element_in_pending_list(false);

  RemoveKeyframeEffectsFromTicking();
}

void ElementAnimations::ElementIdRegistered(ElementId element_id,
                                            ElementListType list_type) {
  DCHECK_EQ(element_id_, element_id);

  bool had_element_in_any_list = has_element_in_any_list();

  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(true);
  else
    set_has_element_in_pending_list(true);

  if (!had_element_in_any_list)
    UpdateKeyframeEffectsTickingState();
}

void ElementAnimations::ElementIdUnregistered(ElementId element_id,
                                              ElementListType list_type) {
  DCHECK_EQ(this->element_id(), element_id);
  if (list_type == ElementListType::ACTIVE)
    set_has_element_in_active_list(false);
  else
    set_has_element_in_pending_list(false);
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

void ElementAnimations::UpdateKeyframeEffectsTickingState() const {
  for (auto& keyframe_effect : keyframe_effects_list_)
    keyframe_effect.UpdateTickingState();
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

bool ElementAnimations::AnimationsPreserveAxisAlignment() const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (!keyframe_effect.AnimationsPreserveAxisAlignment())
      return false;
  }
  return true;
}

void ElementAnimations::GetAnimationScales(ElementListType list_type,
                                           float* maximum_scale,
                                           float* starting_scale) const {
  *maximum_scale = kNotScaled;
  *starting_scale = kNotScaled;
  for (auto& keyframe_effect : keyframe_effects_list_) {
    float keyframe_effect_maximum_scale = kNotScaled;
    float keyframe_effect_starting_scale = kNotScaled;
    bool success = keyframe_effect.GetAnimationScales(
        list_type, &keyframe_effect_maximum_scale,
        &keyframe_effect_starting_scale);
    if (!success) {
      *maximum_scale = kNotScaled;
      *starting_scale = kNotScaled;
      return;
    }
    *maximum_scale = std::max(*maximum_scale, keyframe_effect_maximum_scale);
    *starting_scale = std::max(*starting_scale, keyframe_effect_starting_scale);
  }
}

bool ElementAnimations::ScrollOffsetAnimationWasInterrupted() const {
  for (auto& keyframe_effect : keyframe_effects_list_) {
    if (keyframe_effect.scroll_offset_animation_was_interrupted())
      return true;
  }
  return false;
}

void ElementAnimations::NotifyClientFloatAnimated(
    float value,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  switch (keyframe_model->target_property_id()) {
    case TargetProperty::CSS_CUSTOM_PROPERTY:
      // Custom properties are only tracked on the pending tree, where they may
      // be used as inputs for PaintWorklets (which are only dispatched from the
      // pending tree). As such, we don't need to notify in the case where a
      // KeyframeModel only affects active elements.
      if (KeyframeModelAffectsPendingElements(keyframe_model))
        OnCustomPropertyAnimated(PaintWorkletInput::PropertyValue(value),
                                 keyframe_model);
      break;
    case TargetProperty::OPACITY: {
      float opacity = base::ClampToRange(value, 0.0f, 1.0f);
      if (KeyframeModelAffectsActiveElements(keyframe_model))
        OnOpacityAnimated(ElementListType::ACTIVE, opacity, keyframe_model);
      if (KeyframeModelAffectsPendingElements(keyframe_model))
        OnOpacityAnimated(ElementListType::PENDING, opacity, keyframe_model);
      break;
    }
    default:
      NOTREACHED();
  }
}

void ElementAnimations::NotifyClientFilterAnimated(
    const FilterOperations& filters,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  switch (keyframe_model->target_property_id()) {
    case TargetProperty::BACKDROP_FILTER:
      if (KeyframeModelAffectsActiveElements(keyframe_model))
        OnBackdropFilterAnimated(ElementListType::ACTIVE, filters,
                                 keyframe_model);
      if (KeyframeModelAffectsPendingElements(keyframe_model))
        OnBackdropFilterAnimated(ElementListType::PENDING, filters,
                                 keyframe_model);
      break;
    case TargetProperty::FILTER:
      if (KeyframeModelAffectsActiveElements(keyframe_model))
        OnFilterAnimated(ElementListType::ACTIVE, filters, keyframe_model);
      if (KeyframeModelAffectsPendingElements(keyframe_model))
        OnFilterAnimated(ElementListType::PENDING, filters, keyframe_model);
      break;
    default:
      NOTREACHED();
  }
}

void ElementAnimations::NotifyClientColorAnimated(
    SkColor value,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  DCHECK_EQ(keyframe_model->target_property_id(),
            TargetProperty::CSS_CUSTOM_PROPERTY);
  OnCustomPropertyAnimated(PaintWorkletInput::PropertyValue(value),
                           keyframe_model);
}

void ElementAnimations::NotifyClientTransformOperationsAnimated(
    const TransformOperations& operations,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  gfx::Transform transform = operations.Apply();
  if (KeyframeModelAffectsActiveElements(keyframe_model))
    OnTransformAnimated(ElementListType::ACTIVE, transform, keyframe_model);
  if (KeyframeModelAffectsPendingElements(keyframe_model))
    OnTransformAnimated(ElementListType::PENDING, transform, keyframe_model);
}

void ElementAnimations::NotifyClientScrollOffsetAnimated(
    const gfx::ScrollOffset& scroll_offset,
    int target_property_id,
    KeyframeModel* keyframe_model) {
  if (KeyframeModelAffectsActiveElements(keyframe_model))
    OnScrollOffsetAnimated(ElementListType::ACTIVE, scroll_offset,
                           keyframe_model);
  if (KeyframeModelAffectsPendingElements(keyframe_model))
    OnScrollOffsetAnimated(ElementListType::PENDING, scroll_offset,
                           keyframe_model);
}

void ElementAnimations::InitClientAnimationState() {
  // Clear current states so that UpdateClientAnimationState() will send all
  // (instead of only changed) recalculated current states to the client.
  pending_state_.Clear();
  active_state_.Clear();
  UpdateClientAnimationState();
}

void ElementAnimations::UpdateClientAnimationState() {
  if (!element_id())
    return;
  DCHECK(animation_host_);
  if (!animation_host_->mutator_host_client())
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

  PropertyToElementIdMap element_id_map = GetPropertyToElementIdMap();
  ElementId transform_element_id = element_id_map[TargetProperty::TRANSFORM];

  if (has_element_in_active_list()) {
    if (prev_active != active_state_) {
      PropertyAnimationState diff_active = prev_active ^ active_state_;
      animation_host_->mutator_host_client()->ElementIsAnimatingChanged(
          element_id_map, ElementListType::ACTIVE, diff_active, active_state_);
    }

    float maximum_scale = kNotScaled;
    float starting_scale = kNotScaled;
    if (transform_element_id) {
      GetAnimationScales(ElementListType::ACTIVE, &maximum_scale,
                         &starting_scale);
    }
    if (maximum_scale != active_maximum_scale_ ||
        starting_scale != active_starting_scale_) {
      animation_host_->mutator_host_client()->AnimationScalesChanged(
          transform_element_id, ElementListType::ACTIVE, maximum_scale,
          starting_scale);
      active_maximum_scale_ = maximum_scale;
      active_starting_scale_ = starting_scale;
    }
  }

  if (has_element_in_pending_list()) {
    if (prev_pending != pending_state_) {
      PropertyAnimationState diff_pending = prev_pending ^ pending_state_;
      animation_host_->mutator_host_client()->ElementIsAnimatingChanged(
          element_id_map, ElementListType::PENDING, diff_pending,
          pending_state_);
    }

    float maximum_scale = kNotScaled;
    float starting_scale = kNotScaled;
    if (transform_element_id) {
      GetAnimationScales(ElementListType::PENDING, &maximum_scale,
                         &starting_scale);
    }
    if (maximum_scale != pending_maximum_scale_ ||
        starting_scale != pending_starting_scale_) {
      animation_host_->mutator_host_client()->AnimationScalesChanged(
          transform_element_id, ElementListType::PENDING, maximum_scale,
          starting_scale);
      pending_maximum_scale_ = maximum_scale;
      pending_starting_scale_ = starting_scale;
    }
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
                                         const FilterOperations& filters,
                                         KeyframeModel* keyframe_model) {
  ElementId target_element_id = CalculateTargetElementId(this, keyframe_model);
  DCHECK(target_element_id);
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->SetElementFilterMutated(
      target_element_id, list_type, filters);
}

void ElementAnimations::OnBackdropFilterAnimated(
    ElementListType list_type,
    const FilterOperations& backdrop_filters,
    KeyframeModel* keyframe_model) {
  ElementId target_element_id = CalculateTargetElementId(this, keyframe_model);
  DCHECK(target_element_id);
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->SetElementBackdropFilterMutated(
      target_element_id, list_type, backdrop_filters);
}

void ElementAnimations::OnOpacityAnimated(ElementListType list_type,
                                          float opacity,
                                          KeyframeModel* keyframe_model) {
  ElementId target_element_id = CalculateTargetElementId(this, keyframe_model);
  DCHECK(target_element_id);
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->SetElementOpacityMutated(
      target_element_id, list_type, opacity);
}

void ElementAnimations::OnCustomPropertyAnimated(
    PaintWorkletInput::PropertyValue custom_prop_value,
    KeyframeModel* keyframe_model) {
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->OnCustomPropertyMutated(
      CalculateTargetElementId(this, keyframe_model),
      keyframe_model->custom_property_name(), std::move(custom_prop_value));
}

void ElementAnimations::OnTransformAnimated(ElementListType list_type,
                                            const gfx::Transform& transform,
                                            KeyframeModel* keyframe_model) {
  ElementId target_element_id = CalculateTargetElementId(this, keyframe_model);
  DCHECK(target_element_id);
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->SetElementTransformMutated(
      target_element_id, list_type, transform);
}

void ElementAnimations::OnScrollOffsetAnimated(
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset,
    KeyframeModel* keyframe_model) {
  ElementId target_element_id = CalculateTargetElementId(this, keyframe_model);
  DCHECK(target_element_id);
  DCHECK(animation_host_);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->SetElementScrollOffsetMutated(
      target_element_id, list_type, scroll_offset);
}

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  if (animation_host_) {
    DCHECK(animation_host_->mutator_host_client());
    return animation_host_->mutator_host_client()->GetScrollOffsetForAnimation(
        element_id());
  }

  return gfx::ScrollOffset();
}

PropertyToElementIdMap ElementAnimations::GetPropertyToElementIdMap() const {
  // As noted in the header documentation, this method assumes that each
  // property type maps to at most one ElementId. This is not conceptually true
  // for cc/animations, but it is true for the current clients:
  //
  //   * ui/ does not set per-keyframe-model ElementIds, so this map will be
  //   each property type mapping to the same ElementId (i.e. element_id()).
  //
  //   * blink guarantees that any two keyframe models that it creates which
  //   target the same property on the same target will have the same ElementId.
  //
  // In order to make this as little of a footgun as possible for future-us,
  // this method DCHECKs that the assumption holds.

  std::vector<PropertyToElementIdMap::value_type> entries;
  for (int property_index = TargetProperty::FIRST_TARGET_PROPERTY;
       property_index <= TargetProperty::LAST_TARGET_PROPERTY;
       ++property_index) {
    TargetProperty::Type property =
        static_cast<TargetProperty::Type>(property_index);
    ElementId element_id_for_property;
    for (auto& keyframe_effect : keyframe_effects_list_) {
      KeyframeModel* model = keyframe_effect.GetKeyframeModel(property);
      if (model) {
        // We deliberately use two branches here so that the DCHECK can
        // differentiate between models with different element ids, and the case
        // where some models don't have an element id.
        // TODO(crbug.com/900241): All KeyframeModels should have an ElementId.
        if (model->element_id()) {
          DCHECK(!element_id_for_property ||
                 element_id_for_property == model->element_id())
              << "Different KeyframeModels for the same target must have the "
              << "same ElementId";
          element_id_for_property = model->element_id();
        } else {
          // This DCHECK isn't perfect; you could have a case where one model
          // has an ElementId and the other doesn't, but model->element_id() ==
          // this->element_id() and so the DCHECK passes. That is unlikely
          // enough that we don't bother guarding against it specifically.
          DCHECK(!element_id_for_property ||
                 element_id_for_property == element_id())
              << "Either all models should have an ElementId or none should";
          element_id_for_property = element_id();
        }
      }
    }

    if (element_id_for_property)
      entries.emplace_back(property, element_id_for_property);
  }

  return PropertyToElementIdMap(std::move(entries));
}

unsigned int ElementAnimations::CountKeyframesForTesting() const {
  unsigned int count = 0;
  for (auto it = keyframe_effects_list_.begin();
       it != keyframe_effects_list_.end(); it++)
    count++;
  return count;
}

KeyframeEffect* ElementAnimations::FirstKeyframeEffectForTesting() const {
  DCHECK(keyframe_effects_list_.might_have_observers());
  return &*keyframe_effects_list_.begin();
}

bool ElementAnimations::HasKeyframeEffectForTesting(
    const KeyframeEffect* keyframe) const {
  return keyframe_effects_list_.HasObserver(keyframe);
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
