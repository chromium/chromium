// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ELEMENT_ANIMATIONS_H_
#define CC_ANIMATION_ELEMENT_ANIMATIONS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/filter_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_input.h"
#include "cc/trees/property_animation_state.h"
#include "cc/trees/target_property.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/target_property.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {
class TransformOperations;
}  // namespace gfx

namespace cc {

class AnimationHost;
class FilterOperations;
class KeyframeEffect;
class KeyframeModel;
enum class ElementListType;

// An ElementAnimations owns a list of all KeyframeEffects attached to a single
// target (represented by an ElementId).
//
// Note that a particular target may not actually be an element in the web sense
// of the word; this naming is a legacy leftover. A target is just an amorphous
// blob that has properties that can be animated.
class CC_ANIMATION_EXPORT ElementAnimations
    : public gfx::FloatAnimationCurve::Target,
      public gfx::ColorAnimationCurve::Target,
      public gfx::TransformAnimationCurve::Target,
      public ScrollOffsetAnimationCurve::Target,
      public FilterAnimationCurve::Target,
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

  void RemoveKeyframeEffects();

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

  bool HasAnyAnimationTargetingProperty(TargetProperty::Type property,
                                        ElementId element_id) const;

  // Returns true if there is an animation that is either currently animating
  // the given property or scheduled to animate this property in the future, and
  // that affects the given tree type.
  bool IsPotentiallyAnimatingProperty(TargetProperty::Type target_property,
                                      ElementListType list_type) const;

  // Returns true if there is an animation that is currently animating the given
  // property and that affects the given tree type.
  bool IsCurrentlyAnimatingProperty(TargetProperty::Type target_property,
                                    ElementListType list_type) const;

  bool AnimationsPreserveAxisAlignment() const;

  // Returns the maximum scale along any dimension at any destination in active
  // scale animations, or kInvalidScale if there is no active transform
  // animation or the scale cannot be computed.
  float MaximumScale(ElementId element_id, ElementListType list_type) const;

  bool ScrollOffsetAnimationWasInterrupted() const;

  void SetNeedsPushProperties();

  // Initializes client animation state by calling client's
  // ElementIsAnimatingChanged() method with the current animation state.
  void InitClientAnimationState();
  // Updates client animation state by calling client's
  // ElementIsAnimatingChanged() method with the state containing properties
  // that have changed since the last update.
  void UpdateClientAnimationState();

  // TODO(crbug.com/40747850): Animation targets should be attached to curves
  // when they're created and the concrete subclass is known. This function
  // exists as a stopgap: the animation machinery previously expected to
  // announce a target and then pass curves that would implicitly animate the
  // target (i.e., the machinery handled the attachment).
  void AttachToCurve(gfx::AnimationCurve* c);

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;
  void OnFilterAnimated(const FilterOperations& filter,
                        int target_property_id,
                        gfx::KeyframeModel* keyframe_model) override;
  void OnColorAnimated(const SkColor& color,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;
  void OnTransformAnimated(const gfx::TransformOperations& operations,
                           int target_property_id,
                           gfx::KeyframeModel* keyframe_model) override;
  void OnScrollOffsetAnimated(const gfx::PointF& scroll_offset,
                              int target_property_id,
                              gfx::KeyframeModel* keyframe_model) override;

  std::optional<gfx::PointF> ScrollOffsetForAnimation() const;

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
                        gfx::KeyframeModel* keyframe_model);
  void OnBackdropFilterAnimated(ElementListType list_type,
                                const FilterOperations& backdrop_filters,
                                gfx::KeyframeModel* keyframe_model);
  void OnOpacityAnimated(ElementListType list_type,
                         float opacity,
                         gfx::KeyframeModel* keyframe_model);
  // In addition to custom property animations, these also represent animations
  // of native properties whose values are known to the Blink PaintWorklet
  // responsible for painting them but not known to the compositor. The
  // compositor animates a simple float progress which is then passed into blink
  // code to interpolate. Unlike other native properties listed above, CC is not
  // capable of drawing interpolations of these properties and defers to
  // NativePaintWorklet subclasses to interpret the animation progress as it
  // pertains to how to paint the native property.
  void OnCustomPropertyAnimated(PaintWorkletInput::PropertyValue property_value,
                                KeyframeModel* keyframe_model,
                                int target_property_id);
  void OnTransformAnimated(ElementListType list_type,
                           const gfx::Transform& transform,
                           gfx::KeyframeModel* keyframe_model);
  void OnScrollOffsetAnimated(ElementListType list_type,
                              const gfx::PointF& scroll_offset,
                              gfx::KeyframeModel* keyframe_model);

  static gfx::TargetProperties GetPropertiesMaskForAnimationState();

  void UpdateMaximumScale(ElementId element_id,
                          ElementListType list_type,
                          float* cached_scale);

  void UpdateKeyframeEffectsTickingState() const;
  void RemoveKeyframeEffectsFromTicking() const;

  bool KeyframeModelAffectsActiveElements(
      gfx::KeyframeModel* keyframe_model) const;
  bool KeyframeModelAffectsPendingElements(
      gfx::KeyframeModel* keyframe_model) const;

  base::ObserverList<KeyframeEffect>::Unchecked keyframe_effects_list_;
  raw_ptr<AnimationHost> animation_host_;
  ElementId element_id_;

  mutable bool needs_push_properties_;

  PropertyAnimationState active_state_;
  PropertyAnimationState pending_state_;
  float transform_property_active_maximum_scale_;
  float transform_property_pending_maximum_scale_;
  float scale_property_active_maximum_scale_;
  float scale_property_pending_maximum_scale_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ELEMENT_ANIMATIONS_H_
