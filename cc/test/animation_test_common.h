// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframe_model.h"
#include "cc/animation/transform_operations.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/test/geometry_test_utils.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class FakeFloatAnimationCurve : public FloatAnimationCurve {
 public:
  FakeFloatAnimationCurve();
  explicit FakeFloatAnimationCurve(double duration);
  ~FakeFloatAnimationCurve() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta now) const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeTransformTransition : public TransformAnimationCurve {
 public:
  explicit FakeTransformTransition(double duration);
  ~FakeTransformTransition() override;

  base::TimeDelta Duration() const override;
  TransformOperations GetValue(base::TimeDelta time) const override;
  bool IsTranslation() const override;
  bool PreservesAxisAlignment() const override;
  bool AnimationStartScale(bool forward_direction,
                           float* start_scale) const override;
  bool MaximumTargetScale(bool forward_direction,
                          float* max_scale) const override;

  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeFloatTransition : public FloatAnimationCurve {
 public:
  FakeFloatTransition(double duration, float from, float to);
  ~FakeFloatTransition() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta time) const override;

  std::unique_ptr<AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
  float from_;
  float to_;
};

int AddScrollOffsetAnimationToAnimation(Animation* animation,
                                        gfx::ScrollOffset initial_value,
                                        gfx::ScrollOffset target_value,
                                        KeyframeEffectId effect_id = 0);

int AddAnimatedTransformToAnimation(Animation* animation,
                                    double duration,
                                    int delta_x,
                                    int delta_y,
                                    KeyframeEffectId effect_id = 0);

int AddAnimatedTransformToAnimation(Animation* animation,
                                    double duration,
                                    TransformOperations start_operations,
                                    TransformOperations operations,
                                    KeyframeEffectId effect_id = 0);

int AddOpacityTransitionToAnimation(Animation* animation,
                                    double duration,
                                    float start_opacity,
                                    float end_opacity,
                                    bool use_timing_function,
                                    KeyframeEffectId effect_id = 0);

int AddAnimatedFilterToAnimation(Animation* animation,
                                 double duration,
                                 float start_brightness,
                                 float end_brightness,
                                 KeyframeEffectId effect_id = 0);

int AddAnimatedBackdropFilterToAnimation(Animation* animation,
                                         double duration,
                                         float start_invert,
                                         float end_invert,
                                         KeyframeEffectId effect_id = 0);

int AddOpacityStepsToAnimation(Animation* animation,
                               double duration,
                               float start_opacity,
                               float end_opacity,
                               int num_steps,
                               KeyframeEffectId effect_id = 0);

void AddKeyframeModelToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<KeyframeModel> keyframe_model);
void AddKeyframeModelToElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<KeyframeModel> keyframe_model);

void RemoveKeyframeModelFromElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int keyframe_model_id);

KeyframeModel* GetKeyframeModelFromElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int keyframe_model_id);

int AddAnimatedFilterToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_brightness,
    float end_brightness);

int AddAnimatedTransformToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    int delta_x,
    int delta_y);

int AddAnimatedTransformToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    TransformOperations start_operations,
    TransformOperations operations);

int AddOpacityTransitionToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_opacity,
    float end_opacity,
    bool use_timing_function);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
