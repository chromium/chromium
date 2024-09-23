// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/animation_test_common.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

using gfx::KeyframeModel;

namespace cc {

int AddOpacityTransition(Animation* target,
                         double duration,
                         float start_opacity,
                         float end_opacity,
                         bool use_timing_function,
                         int id,
                         std::optional<int> group_id) {
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());

  std::unique_ptr<gfx::TimingFunction> func;
  if (!use_timing_function)
    func = gfx::CubicBezierTimingFunction::CreatePreset(
        gfx::CubicBezierTimingFunction::EaseType::EASE);
  if (duration > 0.0)
    curve->AddKeyframe(gfx::FloatKeyframe::Create(
        base::TimeDelta(), start_opacity, std::move(func)));
  curve->AddKeyframe(gfx::FloatKeyframe::Create(base::Seconds(duration),
                                                end_opacity, nullptr));

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id,
      group_id ? *group_id : AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
  keyframe_model->set_needs_synchronized_start_time(true);

  target->AddKeyframeModel(std::move(keyframe_model));
  return id;
}

int AddAnimatedTransform(Animation* target,
                         double duration,
                         gfx::TransformOperations start_operations,
                         gfx::TransformOperations operations) {
  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());

  if (duration > 0.0) {
    curve->AddKeyframe(gfx::TransformKeyframe::Create(
        base::TimeDelta(), start_operations, nullptr));
  }

  curve->AddKeyframe(gfx::TransformKeyframe::Create(base::Seconds(duration),
                                                    operations, nullptr));

  int id = AnimationIdProvider::NextKeyframeModelId();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id, AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::TRANSFORM)));
  keyframe_model->set_needs_synchronized_start_time(true);

  target->AddKeyframeModel(std::move(keyframe_model));
  return id;
}

int AddAnimatedCustomProperty(Animation* target,
                              double duration,
                              int start_value,
                              int end_value) {
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());

  if (duration > 0.0) {
    curve->AddKeyframe(
        gfx::FloatKeyframe::Create(base::TimeDelta(), start_value, nullptr));
  }

  curve->AddKeyframe(
      gfx::FloatKeyframe::Create(base::Seconds(duration), end_value, nullptr));

  int id = AnimationIdProvider::NextKeyframeModelId();
  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id, AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::CSS_CUSTOM_PROPERTY)));
  keyframe_model->set_needs_synchronized_start_time(true);

  target->AddKeyframeModel(std::move(keyframe_model));
  return id;
}

int AddAnimatedTransform(Animation* target,
                         double duration,
                         int delta_x,
                         int delta_y) {
  gfx::TransformOperations start_operations;
  if (duration > 0.0) {
    start_operations.AppendTranslate(0, 0, 0.0);
  }

  gfx::TransformOperations operations;
  operations.AppendTranslate(delta_x, delta_y, 0.0);
  return AddAnimatedTransform(target, duration, start_operations, operations);
}

int AddAnimatedFilter(Animation* target,
                      double duration,
                      float start_brightness,
                      float end_brightness) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  if (duration > 0.0) {
    FilterOperations start_filters;
    start_filters.Append(
        FilterOperation::CreateBrightnessFilter(start_brightness));
    curve->AddKeyframe(
        FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  }

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBrightnessFilter(end_brightness));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(duration), filters, nullptr));

  int id = AnimationIdProvider::NextKeyframeModelId();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id, AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::FILTER)));
  keyframe_model->set_needs_synchronized_start_time(true);

  target->AddKeyframeModel(std::move(keyframe_model));
  return id;
}

int AddAnimatedBackdropFilter(Animation* target,
                              double duration,
                              float start_invert,
                              float end_invert) {
  std::unique_ptr<KeyframedFilterAnimationCurve> curve(
      KeyframedFilterAnimationCurve::Create());

  if (duration > 0.0) {
    FilterOperations start_filters;
    start_filters.Append(FilterOperation::CreateInvertFilter(start_invert));
    curve->AddKeyframe(
        FilterKeyframe::Create(base::TimeDelta(), start_filters, nullptr));
  }

  FilterOperations filters;
  filters.Append(FilterOperation::CreateInvertFilter(end_invert));
  curve->AddKeyframe(
      FilterKeyframe::Create(base::Seconds(duration), filters, nullptr));

  int id = AnimationIdProvider::NextKeyframeModelId();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id, AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::BACKDROP_FILTER)));
  keyframe_model->set_needs_synchronized_start_time(true);

  target->AddKeyframeModel(std::move(keyframe_model));
  return id;
}

FakeFloatAnimationCurve::FakeFloatAnimationCurve()
    : duration_(base::Seconds(1.0)) {}

FakeFloatAnimationCurve::FakeFloatAnimationCurve(double duration)
    : duration_(base::Seconds(duration)) {}

FakeFloatAnimationCurve::~FakeFloatAnimationCurve() = default;

base::TimeDelta FakeFloatAnimationCurve::Duration() const {
  return duration_;
}

float FakeFloatAnimationCurve::GetValue(base::TimeDelta now) const {
  return 0.0f;
}

float FakeFloatAnimationCurve::GetTransformedValue(
    base::TimeDelta now,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  return GetValue(now);
}

std::unique_ptr<gfx::AnimationCurve> FakeFloatAnimationCurve::Clone() const {
  return base::WrapUnique(new FakeFloatAnimationCurve);
}

FakeTransformTransition::FakeTransformTransition(double duration)
    : duration_(base::Seconds(duration)) {}

FakeTransformTransition::~FakeTransformTransition() = default;

base::TimeDelta FakeTransformTransition::Duration() const {
  return duration_;
}

gfx::TransformOperations FakeTransformTransition::GetValue(
    base::TimeDelta time) const {
  return gfx::TransformOperations();
}
gfx::TransformOperations FakeTransformTransition::GetTransformedValue(
    base::TimeDelta time,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  return GetValue(time);
}

bool FakeTransformTransition::PreservesAxisAlignment() const {
  return true;
}

bool FakeTransformTransition::MaximumScale(float* max_scale) const {
  *max_scale = 1.f;
  return true;
}

std::unique_ptr<gfx::AnimationCurve> FakeTransformTransition::Clone() const {
  return base::WrapUnique(new FakeTransformTransition(*this));
}

FakeFloatTransition::FakeFloatTransition(double duration, float from, float to)
    : duration_(base::Seconds(duration)), from_(from), to_(to) {}

FakeFloatTransition::~FakeFloatTransition() = default;

base::TimeDelta FakeFloatTransition::Duration() const {
  return duration_;
}

float FakeFloatTransition::GetValue(base::TimeDelta time) const {
  const double progress = std::min(time / duration_, 1.0);
  return (1.0 - progress) * from_ + progress * to_;
}

float FakeFloatTransition::GetTransformedValue(
    base::TimeDelta time,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  return GetValue(time);
}

std::unique_ptr<gfx::AnimationCurve> FakeFloatTransition::Clone() const {
  return base::WrapUnique(new FakeFloatTransition(*this));
}

int AddScrollOffsetAnimationToAnimation(Animation* animation,
                                        gfx::PointF initial_value,
                                        gfx::PointF target_value) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve(
      ScrollOffsetAnimationCurveFactory::CreateEaseInOutAnimationForTesting(
          target_value));
  curve->SetInitialValue(initial_value);

  int id = AnimationIdProvider::NextKeyframeModelId();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id, AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET)));
  keyframe_model->SetIsImplOnly();

  animation->AddKeyframeModel(std::move(keyframe_model));

  return id;
}

int AddAnimatedCustomPropertyToAnimation(Animation* animation,
                                         double duration,
                                         int start_value,
                                         int end_value) {
  return AddAnimatedCustomProperty(animation, duration, start_value, end_value);
}

int AddAnimatedTransformToAnimation(Animation* animation,
                                    double duration,
                                    int delta_x,
                                    int delta_y) {
  return AddAnimatedTransform(animation, duration, delta_x, delta_y);
}

int AddAnimatedTransformToAnimation(Animation* animation,
                                    double duration,
                                    gfx::TransformOperations start_operations,
                                    gfx::TransformOperations operations) {
  return AddAnimatedTransform(animation, duration, start_operations,
                              operations);
}

int AddOpacityTransitionToAnimation(Animation* animation,
                                    double duration,
                                    float start_opacity,
                                    float end_opacity,
                                    bool use_timing_function,
                                    std::optional<int> id,
                                    std::optional<int> group_id) {
  return AddOpacityTransition(
      animation, duration, start_opacity, end_opacity, use_timing_function,
      id ? *id : AnimationIdProvider::NextKeyframeModelId(), group_id);
}

int AddAnimatedFilterToAnimation(Animation* animation,
                                 double duration,
                                 float start_brightness,
                                 float end_brightness) {
  return AddAnimatedFilter(animation, duration, start_brightness,
                           end_brightness);
}

int AddAnimatedBackdropFilterToAnimation(Animation* animation,
                                         double duration,
                                         float start_invert,
                                         float end_invert) {
  return AddAnimatedBackdropFilter(animation, duration, start_invert,
                                   end_invert);
}

int AddOpacityStepsToAnimation(Animation* animation,
                               double duration,
                               float start_opacity,
                               float end_opacity,
                               int num_steps) {
  std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve(
      gfx::KeyframedFloatAnimationCurve::Create());

  std::unique_ptr<gfx::TimingFunction> func = gfx::StepsTimingFunction::Create(
      num_steps, gfx::StepsTimingFunction::StepPosition::START);
  if (duration > 0.0)
    curve->AddKeyframe(gfx::FloatKeyframe::Create(
        base::TimeDelta(), start_opacity, std::move(func)));
  curve->AddKeyframe(gfx::FloatKeyframe::Create(base::Seconds(duration),
                                                end_opacity, nullptr));

  int id = AnimationIdProvider::NextKeyframeModelId();

  std::unique_ptr<KeyframeModel> keyframe_model(KeyframeModel::Create(
      std::move(curve), id, AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
  keyframe_model->set_needs_synchronized_start_time(true);

  animation->AddKeyframeModel(std::move(keyframe_model));
  return id;
}

void AddKeyframeModelToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<KeyframeModel> keyframe_model) {
  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline->AttachAnimation(animation);
  animation->AttachElement(element_id);
  DCHECK(animation->keyframe_effect()->element_animations());
  animation->AddKeyframeModel(std::move(keyframe_model));
}

void AddKeyframeModelToElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    std::unique_ptr<KeyframeModel> keyframe_model) {
  scoped_refptr<const ElementAnimations> element_animations =
      timeline->animation_host()->GetElementAnimationsForElementIdForTesting(
          element_id);
  DCHECK(element_animations);
  KeyframeEffect* keyframe_effect =
      element_animations->FirstKeyframeEffectForTesting();
  DCHECK(keyframe_effect);
  keyframe_effect->AddKeyframeModel(std::move(keyframe_model));
}

void RemoveKeyframeModelFromElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int keyframe_model_id) {
  scoped_refptr<const ElementAnimations> element_animations =
      timeline->animation_host()->GetElementAnimationsForElementIdForTesting(
          element_id);
  DCHECK(element_animations);
  KeyframeEffect* keyframe_effect =
      element_animations->FirstKeyframeEffectForTesting();
  DCHECK(keyframe_effect);
  keyframe_effect->RemoveKeyframeModel(keyframe_model_id);
}

KeyframeModel* GetKeyframeModelFromElementWithExistingKeyframeEffect(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    int keyframe_model_id) {
  scoped_refptr<const ElementAnimations> element_animations =
      timeline->animation_host()->GetElementAnimationsForElementIdForTesting(
          element_id);
  DCHECK(element_animations);
  KeyframeEffect* keyframe_effect =
      element_animations->FirstKeyframeEffectForTesting();
  DCHECK(keyframe_effect);
  return KeyframeModel::ToCcKeyframeModel(
      keyframe_effect->GetKeyframeModelById(keyframe_model_id));
}

int AddAnimatedFilterToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_brightness,
    float end_brightness) {
  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline->AttachAnimation(animation);
  animation->AttachElement(element_id);
  DCHECK(animation->keyframe_effect()->element_animations());
  return AddAnimatedFilterToAnimation(animation.get(), duration,
                                      start_brightness, end_brightness);
}

int AddAnimatedTransformToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    int delta_x,
    int delta_y) {
  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline->AttachAnimation(animation);
  animation->AttachElement(element_id);
  DCHECK(animation->keyframe_effect()->element_animations());
  return AddAnimatedTransformToAnimation(animation.get(), duration, delta_x,
                                         delta_y);
}

int AddAnimatedTransformToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    gfx::TransformOperations start_operations,
    gfx::TransformOperations operations) {
  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline->AttachAnimation(animation);
  animation->AttachElement(element_id);
  DCHECK(animation->keyframe_effect()->element_animations());
  return AddAnimatedTransformToAnimation(animation.get(), duration,
                                         start_operations, operations);
}

int AddOpacityTransitionToElementWithAnimation(
    ElementId element_id,
    scoped_refptr<AnimationTimeline> timeline,
    double duration,
    float start_opacity,
    float end_opacity,
    bool use_timing_function) {
  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline->AttachAnimation(animation);
  animation->AttachElement(element_id);
  DCHECK(animation->keyframe_effect()->element_animations());
  return AddOpacityTransitionToAnimation(animation.get(), duration,
                                         start_opacity, end_opacity,
                                         use_timing_function);
}

scoped_refptr<Animation> CancelAndReplaceAnimation(Animation& animation) {
  int id = animation.id();
  AnimationTimeline* timeline = animation.animation_timeline();
  ElementId element_id = animation.element_id();

  // Cancel the main thread side animation.
  for (auto& keyframe_model : animation.keyframe_effect()->keyframe_models()) {
    animation.RemoveKeyframeModel(keyframe_model->id());
  }
  animation.set_animation_delegate(nullptr);
  if (timeline) {
    timeline->DetachAnimation(&animation);
  }

  auto replacing_animation = Animation::Create(id);
  replacing_animation->set_is_replacement();

  timeline->AttachAnimation(replacing_animation);
  replacing_animation->AttachElement(element_id);
  return replacing_animation;
}

}  // namespace cc
