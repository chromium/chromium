// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ELEMENT_ANIMATIONS_H_
#define CC_ANIMATION_ELEMENT_ANIMATIONS_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_target.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/trees/property_animation_state.h"
#include "cc/trees/target_property.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace cc {

class AnimationHost;
class FilterOperations;
class KeyframeEffect;
class TransformOperations;
enum class ElementListType;

// An ElementAnimations owns a list of all KeyframeEffects attached to a single
// target (represented by an ElementId).
//
// Note that a particular target may not actually be an element in the web sense
// of the word; this naming is a legacy leftover. A target is just an amorphous
// blob that has properties that can be animated.
class CC_ANIMATION_EXPORT ElementAnimations
    : public AnimationTarget,
      public base::RefCounted<ElementAnimations> {
 public:
  static scoped_refptr<ElementAnimations> Create(AnimationHost* host,
                                                 ElementId element_id);

  ElementAnimations(const ElementAnimations&) = delete;
  ElementAnimations& operator=(const ElementAnimations&) = delete;

  bool AnimationHostIs(AnimationHost* host) const {
    return animation_host_ == host;
  }
  void ClearAnimationHost() { animation_host_ = nullptr; }

  ElementId element_id() const { return element_id_; }

  void ClearAffectedElementTypes(const PropertyToElementIdMap& element_id_map);

  // Called when |element_id| is available to animate in |list_type|.
  void ElementIdRegistered(ElementId element_id, ElementListType list_type);

  // Called when |element_id| is no longer avialable to animate in |list_type|.
  void ElementIdUnregistered(ElementId element_id, ElementListType list_type);

  void AddKeyframeEffect(KeyframeEffect* keyframe_effect);
  void RemoveKeyframeEffect(KeyframeEffect* keyframe_effect);
  bool IsEmpty() const;

  // Ensures that the list of active animations on the main thread and the impl
  // thread are kept in sync. This function does not take ownership of the impl
  // thread ElementAnimations.
  void PushPropertiesTo(
      scoped_refptr<ElementAnimations> element_animations_impl) const;

  // Returns true if there are any effects that have neither finished nor
  // aborted.
  bool HasTickingKeyframeEffect() const;

  // Returns true if there are any KeyframeModels at all to process.
  bool HasAnyKeyframeModel() const;

  bool HasAnyAnimationTargetingProperty(TargetProperty::Type property) const;

  // Returns true if there is an animation that is either currently animating
  // the given property or scheduled to animate this property in the future, and
  // that affects the given tree type.
  bool IsPotentiallyAnimatingProperty(TargetProperty::Type target_property,
                                      ElementListType list_type) const;

  // Returns true if there is an animation that is currently animating the given
  // property and that affects the given tree type.
  bool IsCurrentlyAnimatingProperty(TargetProperty::Type target_property,
                                    ElementListType list_type) const;

  bool has_element_in_active_list() const {
    return has_element_in_active_list_;
  }
  bool has_element_in_pending_list() const {
    return has_element_in_pending_list_;
  }
  bool has_element_in_any_list() const {
    return has_element_in_active_list_ || has_element_in_pending_list_;
  }

  void set_has_element_in_active_list(bool has_element_in_active_list) {
    has_element_in_active_list_ = has_element_in_active_list;
  }
  void set_has_element_in_pending_list(bool has_element_in_pending_list) {
    has_element_in_pending_list_ = has_element_in_pending_list;
  }

  bool AnimationsPreserveAxisAlignment() const;

  // Gets scales transform animations. On return, |maximum_scale| is the maximum
  // scale along any dimension at any destination in active scale animations,
  // and |starting_scale| is the maximum of starting animation scale along any
  // dimension at any destination in active scale animations. They are set to
  // kNotScaled if there is no active scale animation or the scales cannot be
  // computed.
  void GetAnimationScales(ElementListType list_type,
                          float* maximum_scale,
                          float* starting_scale) const;

  bool ScrollOffsetAnimationWasInterrupted() const;

  void SetNeedsPushProperties();

  // Initializes client animation state by calling client's
  // ElementIsAnimatingChanged() method with the current animation state.
  void InitClientAnimationState();
  // Updates client animation state by calling client's
  // ElementIsAnimatingChanged() method with the state containing properties
  // that have changed since the last update.
  void UpdateClientAnimationState();

  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 KeyframeModel* keyframe_model) override;
  void NotifyClientFilterAnimated(const FilterOperations& filter,
                                  int target_property_id,
                                  KeyframeModel* keyframe_model) override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                KeyframeModel* keyframe_model) override {}
  void NotifyClientColorAnimated(SkColor color,
                                 int target_property_id,
                                 KeyframeModel* keyframe_model) override;
  void NotifyClientTransformOperationsAnimated(
      const TransformOperations& operations,
      int target_property_id,
      KeyframeModel* keyframe_model) override;
  void NotifyClientScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset,
                                        int target_property_id,
                                        KeyframeModel* keyframe_model) override;

  gfx::ScrollOffset ScrollOffsetForAnimation() const;

  // Returns a map of target property to the ElementId for that property, for
  // KeyframeEffects associated with this ElementAnimations.
  //
  // This method makes the assumption that a given target property doesn't map
  // to more than one ElementId. While conceptually this isn't true for
  // cc/animations, it is true for the two current clients (ui/ and blink) and
  // this is required to let BGPT ship (see http://crbug.com/912574).
  PropertyToElementIdMap GetPropertyToElementIdMap() const;

  unsigned int CountKeyframesForTesting() const;
  KeyframeEffect* FirstKeyframeEffectForTesting() const;
  bool HasKeyframeEffectForTesting(const KeyframeEffect* keyframe) const;

 private:
  friend class base::RefCounted<ElementAnimations>;

  ElementAnimations(AnimationHost* host, ElementId element_id);
  ~ElementAnimations() override;

  void InitAffectedElementTypes();

  void OnFilterAnimated(ElementListType list_type,
                        const FilterOperations& filters,
                        KeyframeModel* keyframe_model);
  void OnBackdropFilterAnimated(ElementListType list_type,
                                const FilterOperations& backdrop_filters,
                                KeyframeModel* keyframe_model);
  void OnOpacityAnimated(ElementListType list_type,
                         float opacity,
                         KeyframeModel* keyframe_model);
  void OnCustomPropertyAnimated(
      PaintWorkletInput::PropertyValue custom_prop_value,
      KeyframeModel* keyframe_model);
  void OnTransformAnimated(ElementListType list_type,
                           const gfx::Transform& transform,
                           KeyframeModel* keyframe_model);
  void OnScrollOffsetAnimated(ElementListType list_type,
                              const gfx::ScrollOffset& scroll_offset,
                              KeyframeModel* keyframe_model);

  static TargetProperties GetPropertiesMaskForAnimationState();

  void UpdateKeyframeEffectsTickingState() const;
  void RemoveKeyframeEffectsFromTicking() const;

  bool KeyframeModelAffectsActiveElements(KeyframeModel* keyframe_model) const;
  bool KeyframeModelAffectsPendingElements(KeyframeModel* keyframe_model) const;

  base::ObserverList<KeyframeEffect>::Unchecked keyframe_effects_list_;
  AnimationHost* animation_host_;
  ElementId element_id_;

  bool has_element_in_active_list_;
  bool has_element_in_pending_list_;

  mutable bool needs_push_properties_;

  PropertyAnimationState active_state_;
  PropertyAnimationState pending_state_;
  float active_maximum_scale_;
  float active_starting_scale_;
  float pending_maximum_scale_;
  float pending_starting_scale_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ELEMENT_ANIMATIONS_H_
