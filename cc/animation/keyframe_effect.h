// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_KEYFRAME_EFFECT_H_
#define CC_ANIMATION_KEYFRAME_EFFECT_H_

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/target_property.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {

class Animation;
enum class PauseCondition { kUnconditional, kAfterStart };
struct PropertyAnimationState;

// Specially designed for a custom property animation on a paint worklet
// element. It doesn't require an element id to run on the compositor thread.
// However, our animation system requires the element to be on the property
// tree in order to keep ticking the animation. Therefore, we use a reserved
// element id for this animation so that the compositor animation system
// recognize it. We do not use ElementId because it's an invalid element id.
inline constexpr ElementId kReservedElementIdForPaintWorklet(
    std::numeric_limits<ElementId::InternalValue>::max() - 1);

// A KeyframeEffect owns a group of KeyframeModels for a single target
// (identified by an ElementId). It is responsible for managing the
// KeyframeModels' running states (starting, running, paused, etc), as well as
// ticking the KeyframeModels when it is requested to produce new outputs for a
// given time.
//
// Note that a single KeyframeEffect may not own all the KeyframeModels for a
// given target. KeyframeEffect is only a grouping mechanism for related
// KeyframeModels. The commonality between keyframe models on the same target
// is found via ElementAnimations - there is only one ElementAnimations for a
// given target.
class CC_ANIMATION_EXPORT KeyframeEffect : public gfx::KeyframeEffect {
 public:
  explicit KeyframeEffect(Animation* animation);
  KeyframeEffect(const KeyframeEffect&) = delete;
  virtual ~KeyframeEffect();

  KeyframeEffect& operator=(const KeyframeEffect&) = delete;

  // ElementAnimations object where this controller is listed.
  scoped_refptr<const ElementAnimations> element_animations() const {
    return element_animations_;
  }

  bool has_bound_element_animations() const { return !!element_animations_; }

  bool has_attached_element() const { return !!element_id_; }

  ElementId element_id() const { return element_id_; }

  // Returns true if there are any KeyframeModels at all to process.
  bool has_any_keyframe_model() const { return !keyframe_models().empty(); }

  // When a scroll animation is removed on the main thread, its compositor
  // thread counterpart continues producing scroll deltas until activation.
  // These scroll deltas need to be cleared at activation, so that the active
  // element's scroll offset matches the offset provided by the main thread
  // rather than a combination of this offset and scroll deltas produced by the
  // removed animation. This is to provide the illusion of synchronicity to JS
  // that simultaneously removes an animation and sets the scroll offset.
  bool scroll_offset_animation_was_interrupted() const {
    return scroll_offset_animation_was_interrupted_;
  }

  bool needs_push_properties() const { return needs_push_properties_; }
  void SetNeedsPushProperties();
  void ResetNeedsPushProperties();

  void BindElementAnimations(ElementAnimations* element_animations);
  void UnbindElementAnimations();

  void AttachElement(ElementId element_id);
  void DetachElement();

  bool Tick(base::TimeTicks monotonic_time) override;
  void RemoveFromTicking();

  void UpdateState(bool start_ready_keyframe_models, AnimationEvents* events);
  void UpdateTickingState();

  void Pause(base::TimeTicks timeline_time,
             PauseCondition = PauseCondition::kUnconditional);

  void AddKeyframeModel(
      std::unique_ptr<gfx::KeyframeModel> keyframe_model) override;
  void PauseKeyframeModel(int keyframe_model_id, base::TimeDelta time_offset);
  void AbortKeyframeModel(int keyframe_model_id);
  void AbortKeyframeModelsWithProperty(TargetProperty::Type target_property,
                                       bool needs_completion);

  void ActivateKeyframeModels();

  void KeyframeModelAdded();

  // Dispatches animation event to a keyframe model specified as part of the
  // event. Returns true if the event is dispatched, false otherwise.
  bool DispatchAnimationEventToKeyframeModel(const AnimationEvent& event);

  // Returns true if there are any KeyframeModels that have neither finished
  // nor aborted.
  bool HasTickingKeyframeModel() const;

  bool RequiresInvalidation() const;
  bool AffectsNativeProperty() const;

  bool AnimationsPreserveAxisAlignment() const;

  // Returns the maximum scale along any dimension at any destination in active
  // scale animations, or kInvalidScale if there is no active transform
  // animation or the scale cannot be computed.
  float MaximumScale(ElementId, ElementListType) const;

  // Returns true if there is a keyframe_model that is either currently
  // animating the given property or scheduled to animate this property in the
  // future, and that affects the given tree type.
  bool IsPotentiallyAnimatingProperty(TargetProperty::Type target_property,
                                      ElementListType list_type) const;

  // Returns true if there is a keyframe_model that is currently animating the
  // given property and that affects the given tree type.
  bool IsCurrentlyAnimatingProperty(TargetProperty::Type target_property,
                                    ElementListType list_type) const;

  void GetPropertyAnimationState(PropertyAnimationState* pending_state,
                                 PropertyAnimationState* active_state) const;

  void MarkAbortedKeyframeModelsForDeletion(
      KeyframeEffect* element_keyframe_effect_impl);
  void PurgeKeyframeModelsMarkedForDeletion(bool impl_only);
  void PushNewKeyframeModelsToImplThread(
      KeyframeEffect* element_keyframe_effect_impl) const;
  void RemoveKeyframeModelsCompletedOnMainThread(
      KeyframeEffect* element_keyframe_effect_impl) const;
  // If `replaced_start_time` is provided, it will be set as the start time on
  // this' keyframe models prior to being pushed.
  void PushPropertiesTo(KeyframeEffect* keyframe_effect_impl,
                        std::optional<base::TimeTicks> replaced_start_time);

  std::string KeyframeModelsToString() const;

  // Iterates through all |keyframe_models_| and returns the minimum of their
  // animation curve's tick intervals.
  // Returns 0 if there is a continuous animation which should be ticked as
  // fast as possible.
  base::TimeDelta MinimumTickInterval() const;

  bool awaiting_deletion() { return awaiting_deletion_; }

  void set_replaced_group(int replaced_group) {
    replaced_group_ = replaced_group;
  }

 protected:
  // We override this because we have additional bookkeeping (eg, noting if
  // we've aborted a scroll animation, updating ticking state, sending updates
  // to the impl instance, informing |element_animations_|).
  void RemoveKeyframeModelRange(
      typename KeyframeModels::iterator to_remove_begin,
      typename KeyframeModels::iterator to_remove_end) override;

 private:
  void StartKeyframeModels(base::TimeTicks monotonic_time);
  void PromoteStartedKeyframeModels(AnimationEvents* events);
  void PurgeDeletedKeyframeModels();

  void MarkKeyframeModelsForDeletion(base::TimeTicks, AnimationEvents* events);
  void MarkFinishedKeyframeModels(base::TimeTicks monotonic_time);

  std::optional<gfx::PointF> ScrollOffsetForAnimation() const;
  void GenerateEvent(AnimationEvents* events,
                     const KeyframeModel& keyframe_model,
                     AnimationEvent::Type type,
                     base::TimeTicks monotonic_time);
  void GenerateTakeoverEventForScrollAnimation(
      AnimationEvents* events,
      const KeyframeModel& keyframe_model,
      base::TimeTicks monotonic_time);

  raw_ptr<Animation> animation_;

  ElementId element_id_;

  // element_animations_ is non-null if controller is attached to an element.
  scoped_refptr<ElementAnimations> element_animations_;

  // When an animation is being replaced (see is_replaced_ in animation.h),
  // this is set to the animation group being replaced. Set only on the
  // impl-side KeyframeEffect.
  std::optional<int> replaced_group_;

  // Only try to start KeyframeModels when new keyframe models are added or
  // when the previous attempt at starting KeyframeModels failed to start all
  // KeyframeModels.
  bool needs_to_start_keyframe_models_;

  bool scroll_offset_animation_was_interrupted_;

  bool is_ticking_;
  bool awaiting_deletion_;
  std::optional<base::TimeTicks> last_tick_time_;

  bool needs_push_properties_;
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAME_EFFECT_H_
