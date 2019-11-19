// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SINGLE_KEYFRAME_EFFECT_ANIMATION_H_
#define CC_ANIMATION_SINGLE_KEYFRAME_EFFECT_ANIMATION_H_

#include <vector>

#include <memory>
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"

namespace cc {

class KeyframeEffect;

// SingleKeyframeEffectAnimation is a sub-class of Animation. It
// serves as a bridge between the cc animation clients and cc because we
// previously only supported single effect(keyframe_effect) per animation.
//
// There is a 1:1 relationship between SingleKeyframeEffectAnimation and
// the KeyframeEffect. In general, the base class Animation is a 1:N
// relationship to allow for grouped animations.
//
// TODO(yigu): Deprecate SingleKeyframeEffectAnimation once grouped
// animations are fully supported by all clients.
class CC_ANIMATION_EXPORT SingleKeyframeEffectAnimation : public Animation {
 public:
  static scoped_refptr<SingleKeyframeEffectAnimation> Create(int id);
  scoped_refptr<Animation> CreateImplInstance() const override;

  SingleKeyframeEffectAnimation(const SingleKeyframeEffectAnimation&) = delete;
  SingleKeyframeEffectAnimation& operator=(
      const SingleKeyframeEffectAnimation&) = delete;

  ElementId element_id() const;

  void AttachElement(ElementId element_id);

  KeyframeEffect* keyframe_effect() const;
  void AddKeyframeModel(std::unique_ptr<KeyframeModel> keyframe_model);
  void PauseKeyframeModel(int keyframe_model_id, double time_offset);
  virtual void RemoveKeyframeModel(int keyframe_model_id);
  void AbortKeyframeModel(int keyframe_model_id);

  bool NotifyKeyframeModelFinishedForTesting(
      TargetProperty::Type target_property,
      int group_id);
  KeyframeModel* GetKeyframeModel(TargetProperty::Type target_property) const;

 private:
  friend class base::RefCounted<SingleKeyframeEffectAnimation>;

  KeyframeEffect* GetKeyframeEffect() const;

 protected:
  explicit SingleKeyframeEffectAnimation(int id);
  explicit SingleKeyframeEffectAnimation(int id, size_t keyframe_effect_id);
  explicit SingleKeyframeEffectAnimation(int id,
                                         std::unique_ptr<KeyframeEffect>);

  ~SingleKeyframeEffectAnimation() override;
};

}  // namespace cc

#endif  // CC_ANIMATION_SINGLE_KEYFRAME_EFFECT_ANIMATION_H_
