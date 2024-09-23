// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "cc/trees/property_tree.h"

namespace cc {
namespace {

class AnimationTest : public AnimationTimelinesTest {
 public:
  AnimationTest() = default;
  ~AnimationTest() override = default;
};
// See element_animations_unittest.cc for active/pending observers tests.

TEST_F(AnimationTest, AttachDetachLayerIfTimelineAttached) {
  EXPECT_TRUE(CheckKeyframeEffectTimelineNeedsPushProperties(false));

  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  timeline_->AttachAnimation(animation_);
  EXPECT_FALSE(animation_->element_animations());
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  EXPECT_FALSE(GetImplKeyframeEffectForLayerId(element_id_));

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);

  EXPECT_FALSE(animation_impl_->element_animations());
  EXPECT_FALSE(animation_impl_->keyframe_effect()->element_id());
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());
  EXPECT_FALSE(timeline_->needs_push_properties());

  animation_->AttachElement(element_id_);
  EXPECT_EQ(animation_->keyframe_effect(),
            GetKeyframeEffectForElementId(element_id_));
  EXPECT_TRUE(animation_->element_animations());
  EXPECT_EQ(animation_->keyframe_effect()->element_id(), element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  EXPECT_EQ(animation_impl_->keyframe_effect(),
            GetImplKeyframeEffectForLayerId(element_id_));
  EXPECT_TRUE(animation_impl_->element_animations());
  EXPECT_EQ(animation_impl_->keyframe_effect()->element_id(), element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  animation_->DetachElement();
  EXPECT_FALSE(GetKeyframeEffectForElementId(element_id_));
  EXPECT_FALSE(animation_->element_animations());
  EXPECT_FALSE(animation_->keyframe_effect()->element_id());
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  EXPECT_FALSE(GetImplKeyframeEffectForLayerId(element_id_));
  EXPECT_FALSE(animation_impl_->element_animations());
  EXPECT_FALSE(animation_impl_->keyframe_effect()->element_id());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  timeline_->DetachAnimation(animation_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(animation_->element_animations());
  EXPECT_FALSE(animation_->keyframe_effect()->element_id());
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());
  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);
}

TEST_F(AnimationTest, AttachDetachTimelineIfLayerAttached) {
  host_->AddAnimationTimeline(timeline_);

  EXPECT_FALSE(animation_->keyframe_effect()->element_animations());
  EXPECT_FALSE(animation_->keyframe_effect()->element_id());
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  animation_->AttachElement(element_id_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(GetKeyframeEffectForElementId(element_id_));
  EXPECT_FALSE(animation_->keyframe_effect()->element_animations());
  EXPECT_EQ(animation_->keyframe_effect()->element_id(), element_id_);
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  timeline_->AttachAnimation(animation_);
  EXPECT_EQ(timeline_, animation_->animation_timeline());
  EXPECT_EQ(animation_->keyframe_effect(),
            GetKeyframeEffectForElementId(element_id_));
  EXPECT_TRUE(animation_->keyframe_effect()->element_animations());
  EXPECT_EQ(animation_->keyframe_effect()->element_id(), element_id_);
  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());

  // Removing animation from timeline detaches layer.
  timeline_->DetachAnimation(animation_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(GetKeyframeEffectForElementId(element_id_));
  EXPECT_FALSE(animation_->keyframe_effect()->element_animations());
  EXPECT_FALSE(animation_->keyframe_effect()->element_id());
  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());
}

TEST_F(AnimationTest, HaveInvalidationAndNativePropertyAnimations) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  const float start_value = .7f;
  const float end_value = .3f;

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  AddAnimatedCustomPropertyToAnimation(animation_.get(), duration, start_value,
                                       end_value);
  AddOpacityTransitionToAnimation(animation_.get(), duration, start_opacity,
                                  end_opacity, false);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);
  EXPECT_TRUE(host_->HasInvalidationAnimation());
  EXPECT_TRUE(host_->HasNativePropertyAnimation());
}

TEST_F(AnimationTest, HasInvalidationAnimation) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  const float start_value = .7f;
  const float end_value = .3f;
  const double duration = 1.;

  AddAnimatedCustomPropertyToAnimation(animation_.get(), duration, start_value,
                                       end_value);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);
  EXPECT_TRUE(host_->HasInvalidationAnimation());
  EXPECT_FALSE(host_->HasNativePropertyAnimation());
}

TEST_F(AnimationTest, HasNativePropertyAnimation) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  AddOpacityTransitionToAnimation(animation_.get(), duration, start_opacity,
                                  end_opacity, false);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);
  EXPECT_FALSE(host_->HasInvalidationAnimation());
  EXPECT_TRUE(host_->HasNativePropertyAnimation());
}

TEST_F(AnimationTest, PropertiesMutate) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const float start_brightness = .6f;
  const float end_brightness = .4f;

  const int transform_x = 10;
  const int transform_y = 20;

  const float start_invert = .8f;
  const float end_invert = .6f;

  const double duration = 1.;

  AddOpacityTransitionToAnimation(animation_.get(), duration, start_opacity,
                                  end_opacity, false);

  AddAnimatedTransformToAnimation(animation_.get(), duration, transform_x,
                                  transform_y);
  AddAnimatedFilterToAnimation(animation_.get(), duration, start_brightness,
                               end_brightness);
  AddAnimatedBackdropFilterToAnimation(animation_.get(), duration, start_invert,
                                       end_invert);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::OPACITY));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::TRANSFORM));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::BACKDROP_FILTER));

  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::OPACITY));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::TRANSFORM));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::BACKDROP_FILTER));

  host_impl_->ActivateAnimations(nullptr);

  base::TimeTicks time;
  time += base::Seconds(0.1);
  TickAnimationsTransferEvents(time, 4u);
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  time += base::Seconds(duration);
  TickAnimationsTransferEvents(time, 4u);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         transform_x, transform_y);
  client_.ExpectFilterPropertyMutated(element_id_, ElementListType::ACTIVE,
                                      end_brightness);
  client_.ExpectBackdropFilterPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_invert);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::ACTIVE, transform_x, transform_y);
  client_impl_.ExpectFilterPropertyMutated(element_id_, ElementListType::ACTIVE,
                                           end_brightness);
  client_impl_.ExpectBackdropFilterPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_invert);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::PENDING, transform_x, transform_y);
  client_impl_.ExpectFilterPropertyMutated(
      element_id_, ElementListType::PENDING, end_brightness);
  client_impl_.ExpectBackdropFilterPropertyMutated(
      element_id_, ElementListType::PENDING, end_invert);
}

TEST_F(AnimationTest, AttachTwoAnimationsToOneLayer) {
  TestAnimationDelegate delegate1;
  TestAnimationDelegate delegate2;

  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  scoped_refptr<Animation> animation1 =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  scoped_refptr<Animation> animation2 =
      Animation::Create(AnimationIdProvider::NextAnimationId());

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation1);
  EXPECT_TRUE(timeline_->needs_push_properties());

  timeline_->AttachAnimation(animation2);
  EXPECT_TRUE(timeline_->needs_push_properties());

  animation1->set_animation_delegate(&delegate1);
  animation2->set_animation_delegate(&delegate2);

  // Attach animations to the same layer.
  animation1->AttachElement(element_id_);
  animation2->AttachElement(element_id_);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransitionToAnimation(animation1.get(), duration, start_opacity,
                                  end_opacity, false);
  AddAnimatedTransformToAnimation(animation2.get(), duration, transform_x,
                                  transform_y);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  host_impl_->ActivateAnimations(nullptr);

  EXPECT_FALSE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_FALSE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  base::TimeTicks time;
  time += base::Seconds(0.1);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_TRUE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  EXPECT_FALSE(animation1->keyframe_effect()->needs_push_properties());
  EXPECT_FALSE(animation2->keyframe_effect()->needs_push_properties());

  time += base::Seconds(duration);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.finished());
  EXPECT_TRUE(delegate2.finished());

  EXPECT_TRUE(animation1->keyframe_effect()->needs_push_properties());
  EXPECT_TRUE(animation2->keyframe_effect()->needs_push_properties());

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_.ExpectTransformPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::ACTIVE, transform_x, transform_y);

  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);
  client_impl_.ExpectTransformPropertyMutated(
      element_id_, ElementListType::PENDING, transform_x, transform_y);

  animation1->set_animation_delegate(nullptr);
  animation2->set_animation_delegate(nullptr);
}

TEST_F(AnimationTest, AddRemoveAnimationToNonAttachedAnimation) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  const double duration = 1.;
  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int filter_id =
      AddAnimatedFilterToAnimation(animation_.get(), duration, 0.1f, 0.9f);
  AddOpacityTransitionToAnimation(animation_.get(), duration, start_opacity,
                                  end_opacity, false);

  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);

  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());
  EXPECT_FALSE(animation_->keyframe_effect()->element_animations());
  animation_->RemoveKeyframeModel(filter_id);
  EXPECT_FALSE(animation_->keyframe_effect()->needs_push_properties());

  animation_->AttachElement(element_id_);

  EXPECT_TRUE(animation_->keyframe_effect()->element_animations());
  EXPECT_FALSE(animation_->keyframe_effect()
                   ->element_animations()
                   ->HasAnyAnimationTargetingProperty(TargetProperty::FILTER,
                                                      element_id_));
  EXPECT_TRUE(animation_->keyframe_effect()
                  ->element_animations()
                  ->HasAnyAnimationTargetingProperty(TargetProperty::OPACITY,
                                                     element_id_));
  EXPECT_TRUE(animation_->keyframe_effect()->needs_push_properties());

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::OPACITY));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::OPACITY));

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::FILTER));

  host_impl_->ActivateAnimations(nullptr);

  base::TimeTicks time;
  time += base::Seconds(0.1);
  TickAnimationsTransferEvents(time, 1u);

  time += base::Seconds(duration);
  TickAnimationsTransferEvents(time, 1u);

  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       end_opacity);
  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::ACTIVE, end_opacity);
  client_impl_.ExpectOpacityPropertyMutated(
      element_id_, ElementListType::PENDING, end_opacity);

  EXPECT_FALSE(client_.IsPropertyMutated(element_id_, ElementListType::ACTIVE,
                                         TargetProperty::FILTER));
  EXPECT_FALSE(client_impl_.IsPropertyMutated(
      element_id_, ElementListType::ACTIVE, TargetProperty::FILTER));
}

using AnimationDeathTest = AnimationTest;

TEST_F(AnimationDeathTest, RemoveAddInSameFrame) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  const int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1., .7f, .3f, false);
  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  animation_->RemoveKeyframeModel(keyframe_model_id);
  AddOpacityTransitionToAnimation(animation_.get(), 1., .7f, .3f, false,
                                  keyframe_model_id);
  EXPECT_DCHECK_DEATH(
      host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees()));
}

TEST_F(AnimationTest, AddRemoveAnimationCausesSetNeedsCommit) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  const int keyframe_model_id =
      AddOpacityTransitionToAnimation(animation_.get(), 1., .7f, .3f, false);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  animation_->PauseKeyframeModel(keyframe_model_id, base::Seconds(1));
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  animation_->RemoveKeyframeModel(keyframe_model_id);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);
}

// If main-thread animation switches to another layer within one frame then
// impl-thread animation must be switched as well.
TEST_F(AnimationTest, SwitchToLayer) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);

  EXPECT_EQ(animation_->keyframe_effect(),
            GetKeyframeEffectForElementId(element_id_));
  EXPECT_TRUE(animation_->keyframe_effect()->element_animations());
  EXPECT_EQ(animation_->keyframe_effect()->element_id(), element_id_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);
  EXPECT_EQ(animation_impl_->keyframe_effect(),
            GetImplKeyframeEffectForLayerId(element_id_));
  EXPECT_TRUE(animation_impl_->keyframe_effect()->element_animations());
  EXPECT_EQ(animation_impl_->keyframe_effect()->element_id(), element_id_);
  CheckKeyframeEffectTimelineNeedsPushProperties(false);

  const ElementId new_element_id(element_id_.GetInternalValue() + 1);
  animation_->DetachElement();
  animation_->AttachElement(new_element_id);

  EXPECT_EQ(animation_->keyframe_effect(),
            GetKeyframeEffectForElementId(new_element_id));
  EXPECT_TRUE(animation_->keyframe_effect()->element_animations());
  EXPECT_EQ(animation_->keyframe_effect()->element_id(), new_element_id);
  CheckKeyframeEffectTimelineNeedsPushProperties(true);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  EXPECT_EQ(animation_impl_->keyframe_effect(),
            GetImplKeyframeEffectForLayerId(new_element_id));
  EXPECT_TRUE(animation_impl_->keyframe_effect()->element_animations());
  EXPECT_EQ(animation_impl_->keyframe_effect()->element_id(), new_element_id);
}

TEST_F(AnimationTest, ToString) {
  animation_->AttachElement(element_id_);
  EXPECT_EQ(
      base::StringPrintf("Animation{id=%d, element_id=%s, keyframe_models=[]}",
                         animation_->id(), element_id_.ToString().c_str()),
      animation_->ToString());

  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::make_unique<FakeFloatAnimationCurve>(15), 42, 73,
      KeyframeModel::TargetPropertyId(TargetProperty::OPACITY)));
  EXPECT_EQ(
      base::StringPrintf("Animation{id=%d, element_id=%s, "
                         "keyframe_models=[KeyframeModel{id=42, "
                         "group=73, target_property_type=4, "
                         "custom_property_name=, native_property_type=2, "
                         "run_state=WAITING_FOR_TARGET_AVAILABILITY, "
                         "element_id=(0)}]}",
                         animation_->id(), element_id_.ToString().c_str()),
      animation_->ToString());

  animation_->AddKeyframeModel(KeyframeModel::Create(
      std::make_unique<FakeFloatAnimationCurve>(18), 45, 76,
      KeyframeModel::TargetPropertyId(TargetProperty::BOUNDS)));
  EXPECT_EQ(base::StringPrintf(
                "Animation{id=%d, element_id=%s, "
                "keyframe_models=[KeyframeModel{id=42, "
                "group=73, target_property_type=4, custom_property_name=, "
                "native_property_type=2, "
                "run_state=WAITING_FOR_TARGET_AVAILABILITY, element_id=(0)}, "
                "KeyframeModel{id=45, group=76, target_property_type=8, "
                "custom_property_name=, native_property_type=2, "
                "run_state=WAITING_FOR_TARGET_AVAILABILITY, element_id=(0)}]}",
                animation_->id(), element_id_.ToString().c_str()),
            animation_->ToString());
}

TEST_F(AnimationTest, AnimationReplacementDeletesKeyframeModels) {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);
  animation_->AttachElement(element_id_);

  int group_id = 1;
  int original_model_id = AddOpacityTransitionToAnimation(
      animation_.get(), /*duration=*/1., /*start_opacity=*/1.0f,
      /*end_opacity=*/0.3f, /*use_timing_function=*/false, /*id=*/std::nullopt,
      group_id);

  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  ASSERT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  ASSERT_TRUE(animation_impl_);

  KeyframeModel* original_model_impl = KeyframeModel::ToCcKeyframeModel(
      animation_impl_->keyframe_effect()->keyframe_models().front().get());

  ASSERT_EQ(original_model_impl->id(), original_model_id);

  host_impl_->ActivateAnimations(nullptr);

  base::TimeTicks time;
  time += base::Seconds(0.1);
  TickAnimationsTransferEvents(time, 1u);

  ASSERT_EQ(animation_impl_->keyframe_effect()->keyframe_models().size(), 1ul);
  ASSERT_EQ(original_model_impl->run_state(), gfx::KeyframeModel::RUNNING);

  // An animation replacement reuses the impl-side Animation and KeyframeEffect
  // objects. Ensure that the old keyframe models are cleared out when this
  // happens.
  auto replacing_animation = CancelAndReplaceAnimation(*animation_);
  int replacing_model_id = AddOpacityTransitionToAnimation(
      replacing_animation.get(), /*duration=*/1., /*start_opacity=*/1.0f,
      /*end_opacity=*/0.3f, /*use_timing_function=*/false, /*id=*/std::nullopt,
      group_id);

  // The push properties appends the new keyframe model and notices the
  // original keyframe model no longer exists on the main thread so marks it as
  // not affecting pending elements.
  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  EXPECT_EQ(animation_impl_->keyframe_effect()->keyframe_models().size(), 2ul);
  EXPECT_EQ(original_model_impl->run_state(), gfx::KeyframeModel::RUNNING);
  EXPECT_FALSE(original_model_impl->affects_pending_elements());
  EXPECT_TRUE(original_model_impl->affects_active_elements());

  // Activating it marks it as not affecting any elements and so marks it as
  // finished.
  host_impl_->ActivateAnimations(nullptr);
  EXPECT_EQ(original_model_impl->run_state(),
            gfx::KeyframeModel::WAITING_FOR_DELETION);
  EXPECT_FALSE(original_model_impl->affects_active_elements());
  EXPECT_EQ(animation_impl_->keyframe_effect()->keyframe_models().size(), 2ul);

  // The next time the effect is updated from the main thread, the commit will
  // cause the keyframe model to be purged.
  replacing_animation->keyframe_effect()->SetNeedsPushProperties();
  host_->PushPropertiesTo(host_impl_, client_.GetPropertyTrees());
  EXPECT_EQ(animation_impl_->keyframe_effect()->keyframe_models().size(), 1ul);
  EXPECT_EQ(animation_impl_->keyframe_effect()->keyframe_models().front()->id(),
            replacing_model_id);
}

}  // namespace
}  // namespace cc
