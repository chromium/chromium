// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TEST_COMMON_H_

#include <memory>

#include "base/time/time.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace gfx {
class PointF;
}

namespace cc {

class FakeFloatAnimationCurve : public gfx::FloatAnimationCurve {
 public:
  FakeFloatAnimationCurve();
  explicit FakeFloatAnimationCurve(double duration);
  ~FakeFloatAnimationCurve() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta now) const override;
  float GetTransformedValue(
      base::TimeDelta now,
      gfx::TimingFunction::LimitDirection limit_direction) const override;
  std::unique_ptr<gfx::AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeTransformTransition : public gfx::TransformAnimationCurve {
 public:
  explicit FakeTransformTransition(double duration);
  ~FakeTransformTransition() override;

  base::TimeDelta Duration() const override;
  gfx::TransformOperations GetValue(base::TimeDelta time) const override;
  gfx::TransformOperations GetTransformedValue(
      base::TimeDelta time,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

  bool PreservesAxisAlignment() const override;
  bool MaximumScale(float* max_scale) const override;

  std::unique_ptr<gfx::AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
};

class FakeFloatTransition : public gfx::FloatAnimationCurve {
 public:
  FakeFloatTransition(double duration, float from, float to);
  ~FakeFloatTransition() override;

  base::TimeDelta Duration() const override;
  float GetValue(base::TimeDelta time) const override;
  float GetTransformedValue(
      base::TimeDelta time,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

  std::unique_ptr<gfx::AnimationCurve> Clone() const override;

 private:
  base::TimeDelta duration_;
  float from_;
  float to_;
};

int AddScrollOffsetAnimationToAnimation(Animation* animation,
                                        gfx::PointF initial_value,
                                        gfx::PointF target_value);

int AddAnimatedTransformToAnimation(Animation* animation,
                                    double duration,
                                    int delta_x,
                                    int delta_y);

int AddAnimatedCustomPropertyToAnimation(Animation* animation,
                                         double duration,
                                         int start_value,
                                         int end_value);

int AddAnimatedTransformToAnimation(Animation* animation,
                                    double duration,
                                    gfx::TransformOperations start_operations,
                                    gfx::TransformOperations operations);

int AddOpacityTransitionToAnimation(Animation* animation,
                                    double duration,
                                    float start_opacity,
                                    float end_opacity,
                                    bool use_timing_function,
                                    std::optional<int> id = std::nullopt,
                                    std::optional<int> group_id = std::nullopt);

int AddAnimatedFilterToAnimation(Animation* animation,
                                 double duration,
                                 float start_brightness,
                                 float end_brightness);

int AddAnimatedBackdropFilterToAnimation(Animation* animation,
                                         double duration,
                                         float start_invert,
                                         float end_invert);

int AddOpacityStepsToAnimation(Animation* animation,
                               double duration,
                               float start_opacity,
                               float end_opacity,
                               int num_steps);

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
    gfx::TransformOperations start_operations,
    gfx::TransformOperations operations);

int AddOpacityTransitionToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_opacity,
    float end_opacity,
    bool use_timing_function);

scoped_refptr<Animation> CancelAndReplaceAnimation(Animation& animation);

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TEST_COMMON_H_
