// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_H_
#define CC_ANIMATION_ANIMATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_model.h"
#include "cc/base/protected_sequence_synchronizer.h"
#include "cc/paint/element_id.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"

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
class CC_ANIMATION_EXPORT Animation : public base::RefCounted<Animation>,
                                      public ProtectedSequenceSynchronizer {
 public:
  static scoped_refptr<Animation> Create(int id);
  virtual scoped_refptr<Animation> CreateImplInstance() const;

  Animation(const Animation&) = delete;
  Animation& operator=(const Animation&) = delete;

  int id() const { return id_; }
  ElementId element_id() const;

  KeyframeEffect* keyframe_effect() {
    return keyframe_effect_.Write(*this).get();
  }

  const KeyframeEffect* keyframe_effect() const {
    return keyframe_effect_.Read(*this);
  }

  // Parent AnimationHost. Animation can be detached from AnimationTimeline.
  AnimationHost* animation_host() {
    DCHECK(IsOwnerThread() || InProtectedSequence());
    return animation_host_;
  }
  const AnimationHost* animation_host() const {
    DCHECK(IsOwnerThread() || InProtectedSequence());
    return animation_host_;
  }
  void SetAnimationHost(AnimationHost* animation_host);
  bool has_animation_host() const { return !!animation_host(); }

  // Parent AnimationTimeline.
  AnimationTimeline* animation_timeline() {
    return animation_timeline_.Read(*this);
  }
  const AnimationTimeline* animation_timeline() const {
    return animation_timeline_.Read(*this);
  }
  void SetAnimationTimeline(AnimationTimeline* timeline);

  scoped_refptr<const ElementAnimations> element_animations() const;

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElement(ElementId element_id);
  void AttachPaintWorkletElement();
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
  virtual bool Tick(base::TimeTicks tick_time);
  bool IsScrollLinkedAnimation() const;

  void AddToTicking();
  void RemoveFromTicking();

  // Dispatches animation event to the animation keyframe effect and model when
  // appropriate, based on the event characteristics.
  // Delegates animation event that was successfully dispatched or doesn't need
  // to be dispatched.
  void DispatchAndDelegateAnimationEvent(const AnimationEvent& event);

  // Returns true if this animation effects pending tree, such as a custom
  // property animation with paint worklet.
  bool RequiresInvalidation() const;
  // Returns true if this animation effects active tree, such as a transform
  // animation.
  bool AffectsNativeProperty() const;

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

  void set_is_replacement() { is_replacement_ = true; }

  std::optional<base::TimeTicks> GetStartTime() const;

  virtual bool IsWorkletAnimation() const;

  void SetKeyframeEffectForTesting(std::unique_ptr<KeyframeEffect>);

  // ProtectedSequenceSynchronizer implementation
  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

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
  ~Animation() override;

  raw_ptr<AnimationDelegate> animation_delegate_ = nullptr;

  const int id_;

 private:
  // If this Animation was created to replace an existing one of the same id,
  // it should take the start time from the impl instance before replacing it,
  // since the start time may not yet have been committed back to the client at
  // the time the animation was restarted. The client sets this bit to true
  // when such an animation is created so that the first commit pulls the start
  // time into this Animation before pushing it.
  //
  // When this animation is pushed to the impl thread, it will update the
  // existing Animation and KeyframeEffect rather than creating new ones. It
  // will silently replace the effect's keyframe models with the new ones
  // specified in this animation.
  //
  // Used only from the main thread and isn't synced to the compositor thread.
  bool is_replacement_ = false;

  // Animation's ProtectedSequenceSynchronizer implementation is implemented
  // using this member. As such the various helpers can not be used to protect
  // access (otherwise we would get infinite recursion).
  raw_ptr<AnimationHost> animation_host_ = nullptr;
  ProtectedSequenceReadable<raw_ptr<AnimationTimeline>> animation_timeline_{
      nullptr};
  ProtectedSequenceWritable<std::unique_ptr<KeyframeEffect>> keyframe_effect_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_H_
