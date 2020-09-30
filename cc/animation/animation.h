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
//
// There is a 1:1 relationship between Animation and KeyframeEffect.
class CC_ANIMATION_EXPORT Animation : public base::RefCounted<Animation> {
 public:
  static scoped_refptr<Animation> Create(int id);
  virtual scoped_refptr<Animation> CreateImplInstance() const;

  Animation(const Animation&) = delete;
  Animation& operator=(const Animation&) = delete;

  int id() const { return id_; }
  ElementId element_id() const;

  KeyframeEffect* keyframe_effect() const { return keyframe_effect_.get(); }

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
  void SetAnimationTimeline(AnimationTimeline* timeline);

  scoped_refptr<ElementAnimations> element_animations() const;

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElement(ElementId element_id);
  // Specially designed for a custom property animation on a paint worklet
  // element. It doesn't require an element id to run on the compositor thread.
  // However, our compositor animation system requires the element to be on the
  // property tree in order to keep ticking the animation. Therefore, we use a
  // reserved element id for this animation so that the compositor animation
  // system recognize it. We do not use 0 as the element id because 0 is
  // kInvalidElementId.
  void AttachNoElement();
  void DetachElement();

  void AddKeyframeModel(std::unique_ptr<KeyframeModel> keyframe_model);
  void PauseKeyframeModel(int keyframe_model_id, base::TimeDelta time_offset);
  virtual void RemoveKeyframeModel(int keyframe_model_id);
  void AbortKeyframeModel(int keyframe_model_id);

  void NotifyKeyframeModelFinishedForTesting(
      int timeline_id,
      int keyframe_model_id,
      TargetProperty::Type target_property,
      int group_id);

  void AbortKeyframeModelsWithProperty(TargetProperty::Type target_property,
                                       bool needs_completion);

  virtual void PushPropertiesTo(Animation* animation_impl);

  virtual void UpdateState(bool start_ready_keyframe_models,
                           AnimationEvents* events);
  // Adds TIME_UPDATED event generated in the current frame to the given
  // animation events.
  virtual void TakeTimeUpdatedEvent(AnimationEvents* events) {}
  virtual void Tick(base::TimeTicks tick_time);
  bool IsScrollLinkedAnimation() const;

  void AddToTicking();
  void RemoveFromTicking();

  // Dispatches animation event to the animation keyframe effect and model when
  // appropriate, based on the event characteristics.
  // Delegates animation event that was successfully dispatched or doesn't need
  // to be dispatched.
  void DispatchAndDelegateAnimationEvent(const AnimationEvent& event);

  bool AffectsCustomProperty() const;

  void SetNeedsPushProperties();

  // Make KeyframeModels affect active elements if and only if they affect
  // pending elements. Any KeyframeModels that no longer affect any elements
  // are deleted.
  void ActivateKeyframeModels();

  // Returns the keyframe model animating the given property that is either
  // running, or is next to run, if such a keyframe model exists.
  KeyframeModel* GetKeyframeModel(TargetProperty::Type target_property) const;

  std::string ToString() const;

  void SetNeedsCommit();

  virtual bool IsWorkletAnimation() const;

 private:
  friend class base::RefCounted<Animation>;

  void RegisterAnimation();
  void UnregisterAnimation();

  // Delegates animation event
  void DelegateAnimationEvent(const AnimationEvent& event);

  // Common code between AttachElement and AttachNoElement.
  void AttachElementInternal(ElementId element_id);

 protected:
  explicit Animation(int id);
  Animation(int id, std::unique_ptr<KeyframeEffect>);
  virtual ~Animation();

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  AnimationDelegate* animation_delegate_;

  int id_;
  std::unique_ptr<KeyframeEffect> keyframe_effect_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_H_
