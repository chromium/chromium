// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_H_
#define CC_ANIMATION_ANIMATION_H_

#include <vector>

#include <memory>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"

namespace cc {

class AnimationDelegate;
class AnimationEvents;
class AnimationHost;
class AnimationTimeline;
class KeyframeEffect;
struct AnimationEvent;

// An Animation is responsible for managing animating properties for a set of
// targets. Each target is represented by a KeyframeEffect and can be animating
// multiple properties on that target; see the KeyframeEffect class.
//
// A particular Animation may not own all the KeyframeEffects for a given
// target. Animation is only a grouping mechanism for related effects, and the
// grouping relationship is defined by the client. It is also the client's
// responsibility to deal with any conflicts that arise from animating the same
// property of the same target across multiple Animations.
//
// Each Animation has a copy on the impl thread, and will take care of
// synchronizing to/from the impl thread when requested.
class CC_ANIMATION_EXPORT Animation : public base::RefCounted<Animation> {
 public:
  static scoped_refptr<Animation> Create(int id);
  virtual scoped_refptr<Animation> CreateImplInstance() const;

  Animation(const Animation&) = delete;
  Animation& operator=(const Animation&) = delete;

  int id() const { return id_; }
  typedef size_t KeyframeEffectId;

  // Parent AnimationHost. Animation can be detached from AnimationTimeline.
  AnimationHost* animation_host() { return animation_host_; }
  const AnimationHost* animation_host() const { return animation_host_; }
  void SetAnimationHost(AnimationHost* animation_host);
  bool has_animation_host() const { return !!animation_host_; }

  // Parent AnimationTimeline.
  AnimationTimeline* animation_timeline() { return animation_timeline_; }
  const AnimationTimeline* animation_timeline() const {
    return animation_timeline_;
  }
  virtual void SetAnimationTimeline(AnimationTimeline* timeline);

  // TODO(smcgruer): If/once ScrollTimeline is supported on normal Animations,
  // we will need to move the promotion logic from WorkletAnimation to here.
  virtual void PromoteScrollTimelinePendingToActive() {}

  bool has_element_animations() const;
  scoped_refptr<ElementAnimations> element_animations(
      KeyframeEffectId keyframe_effect_id) const;

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElementForKeyframeEffect(ElementId element_id,
                                      KeyframeEffectId keyframe_effect_id);
  void DetachElementForKeyframeEffect(ElementId element_id,
                                      KeyframeEffectId keyframe_effect_id);
  virtual void DetachElement();

  void AddKeyframeModelForKeyframeEffect(
      std::unique_ptr<KeyframeModel> keyframe_model,
      KeyframeEffectId keyframe_effect_id);
  void PauseKeyframeModelForKeyframeEffect(int keyframe_model_id,
                                           double time_offset,
                                           KeyframeEffectId keyframe_effect_id);
  void RemoveKeyframeModelForKeyframeEffect(
      int keyframe_model_id,
      KeyframeEffectId keyframe_effect_id);
  void AbortKeyframeModelForKeyframeEffect(int keyframe_model_id,
                                           KeyframeEffectId keyframe_effect_id);
  void AbortKeyframeModelsWithProperty(TargetProperty::Type target_property,
                                       bool needs_completion);

  virtual void PushPropertiesTo(Animation* animation_impl);

  virtual void UpdateState(bool start_ready_keyframe_models,
                           AnimationEvents* events);
  virtual void Tick(base::TimeTicks monotonic_time);

  void AddToTicking();
  void RemoveFromTicking();

  // AnimationDelegate routing.
  void NotifyKeyframeModelStarted(const AnimationEvent& event);
  void NotifyKeyframeModelFinished(const AnimationEvent& event);
  void NotifyKeyframeModelAborted(const AnimationEvent& event);
  void NotifyKeyframeModelTakeover(const AnimationEvent& event);
  size_t TickingKeyframeModelsCount() const;
  bool AffectsCustomProperty() const;

  void SetNeedsPushProperties();

  // Make KeyframeModels affect active elements if and only if they affect
  // pending elements. Any KeyframeModels that no longer affect any elements
  // are deleted.
  void ActivateKeyframeEffects();

  // Returns the keyframe model animating the given property that is either
  // running, or is next to run, if such a keyframe model exists.
  KeyframeModel* GetKeyframeModelForKeyframeEffect(
      TargetProperty::Type target_property,
      KeyframeEffectId keyframe_effect_id) const;

  std::string ToString() const;

  void SetNeedsCommit();

  virtual bool IsWorkletAnimation() const;
  void AddKeyframeEffect(std::unique_ptr<KeyframeEffect>);

  KeyframeEffect* GetKeyframeEffectById(
      KeyframeEffectId keyframe_effect_id) const;
  KeyframeEffectId NextKeyframeEffectId() { return keyframe_effects_.size(); }

 private:
  friend class base::RefCounted<Animation>;

  void RegisterKeyframeEffect(ElementId element_id,
                              KeyframeEffectId keyframe_effect_id);
  void UnregisterKeyframeEffect(ElementId element_id,
                                KeyframeEffectId keyframe_effect_id);
  void RegisterKeyframeEffects();
  void UnregisterKeyframeEffects();

  void PushAttachedKeyframeEffectsToImplThread(Animation* animation_impl) const;
  void PushPropertiesToImplThread(Animation* animation_impl);

 protected:
  explicit Animation(int id);
  virtual ~Animation();

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  AnimationDelegate* animation_delegate_;

  int id_;

  using ElementToKeyframeEffectIdMap =
      std::unordered_map<ElementId,
                         std::unordered_set<KeyframeEffectId>,
                         ElementIdHash>;
  using KeyframeEffects = std::vector<std::unique_ptr<KeyframeEffect>>;

  // It is possible for a keyframe_effect to be in keyframe_effects_ but not in
  // element_to_keyframe_effect_id_map_ but the reverse is not possible.
  ElementToKeyframeEffectIdMap element_to_keyframe_effect_id_map_;
  KeyframeEffects keyframe_effects_;

  int ticking_keyframe_effects_count;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_H_
