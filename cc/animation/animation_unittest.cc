// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include <memory>

#include "base/strings/stringprintf.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"

namespace cc {
namespace {

class AnimationTest : public AnimationTimelinesTest {
 public:
  AnimationTest() : animation_(Animation::Create(animation_id_)) {
    keyframe_effect_id_ = animation_->NextKeyframeEffectId();
  }
  ~AnimationTest() override = default;

  void CheckKeyframeEffectAndTimelineNeedsPushProperties(
      bool needs_push_properties,
      KeyframeEffectId keyframe_effect_id) const {
    KeyframeEffect* keyframe_effect =
        animation_->GetKeyframeEffectById(keyframe_effect_id);
    EXPECT_EQ(keyframe_effect->needs_push_properties(), needs_push_properties);
    EXPECT_EQ(timeline_->needs_push_properties(), needs_push_properties);
  }

 protected:
  scoped_refptr<Animation> animation_;
  scoped_refptr<Animation> animation_impl_;
  KeyframeEffectId keyframe_effect_id_;
};
// See element_animations_unittest.cc for active/pending observers tests.

TEST_F(AnimationTest, AttachDetachLayerIfTimelineAttached) {
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());

  timeline_->AttachAnimation(animation_);
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id_));
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(GetImplKeyframeEffectForLayerId(element_id_));

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);

  EXPECT_FALSE(animation_impl_->element_animations(keyframe_effect_id_));
  EXPECT_FALSE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_id());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());
  EXPECT_FALSE(timeline_->needs_push_properties());

  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);
  EXPECT_EQ(animation_->GetKeyframeEffectById(keyframe_effect_id_),
            GetKeyframeEffectForElementId(element_id_));
  EXPECT_TRUE(animation_->element_animations(keyframe_effect_id_));
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      element_id_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(true, keyframe_effect_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_EQ(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_),
            GetImplKeyframeEffectForLayerId(element_id_));
  EXPECT_TRUE(animation_impl_->element_animations(keyframe_effect_id_));
  EXPECT_EQ(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      element_id_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

  animation_->DetachElement();
  EXPECT_FALSE(GetKeyframeEffectForElementId(element_id_));
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id_));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id());
  CheckKeyframeEffectAndTimelineNeedsPushProperties(true, keyframe_effect_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(GetImplKeyframeEffectForLayerId(element_id_));
  EXPECT_FALSE(animation_impl_->element_animations(keyframe_effect_id_));
  EXPECT_FALSE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_id());
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

  timeline_->DetachAnimation(animation_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id_));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id());
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());
  host_->PushPropertiesTo(host_impl_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);
}

TEST_F(AnimationTest, AttachDetachTimelineIfLayerAttached) {
  host_->AddAnimationTimeline(timeline_);

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_));

  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_animations());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());

  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(GetKeyframeEffectForElementId(element_id_));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_animations());
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      element_id_);
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());

  timeline_->AttachAnimation(animation_);
  EXPECT_EQ(timeline_, animation_->animation_timeline());
  EXPECT_EQ(animation_->GetKeyframeEffectById(keyframe_effect_id_),
            GetKeyframeEffectForElementId(element_id_));
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations());
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      element_id_);
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->needs_push_properties());

  // Removing animation from timeline detaches layer.
  timeline_->DetachAnimation(animation_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(GetKeyframeEffectForElementId(element_id_));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_animations());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->needs_push_properties());
}

TEST_F(AnimationTest, PropertiesMutate) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  host_->AddAnimationTimeline(timeline_);

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_));

  timeline_->AttachAnimation(animation_);
  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(true, keyframe_effect_id_);

  host_->PushPropertiesTo(host_impl_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

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
                                  end_opacity, false, keyframe_effect_id_);

  AddAnimatedTransformToAnimation(animation_.get(), duration, transform_x,
                                  transform_y, keyframe_effect_id_);
  AddAnimatedFilterToAnimation(animation_.get(), duration, start_brightness,
                               end_brightness, keyframe_effect_id_);
  AddAnimatedBackdropFilterToAnimation(animation_.get(), duration, start_invert,
                                       end_invert, keyframe_effect_id_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(true, keyframe_effect_id_);

  host_->PushPropertiesTo(host_impl_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

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
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 4u);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 4u);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(true, keyframe_effect_id_);

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

  scoped_refptr<Animation> animation1 = Animation::Create(100);
  scoped_refptr<Animation> animation2 = Animation::Create(200);

  KeyframeEffectId keyframe_effect_id1 = animation1->NextKeyframeEffectId();
  animation1->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id1));
  ASSERT_TRUE(animation1->GetKeyframeEffectById(keyframe_effect_id1));
  KeyframeEffectId keyframe_effect_id2 = animation2->NextKeyframeEffectId();
  animation2->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id2));
  ASSERT_TRUE(animation2->GetKeyframeEffectById(keyframe_effect_id2));

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation1);
  EXPECT_TRUE(timeline_->needs_push_properties());

  timeline_->AttachAnimation(animation2);
  EXPECT_TRUE(timeline_->needs_push_properties());

  animation1->set_animation_delegate(&delegate1);
  animation2->set_animation_delegate(&delegate2);

  // Attach animations to the same layer.
  animation1->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id1);
  animation2->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id2);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransitionToAnimation(animation1.get(), duration, start_opacity,
                                  end_opacity, false, keyframe_effect_id1);
  AddAnimatedTransformToAnimation(animation2.get(), duration, transform_x,
                                  transform_y, keyframe_effect_id2);

  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations(nullptr);

  EXPECT_FALSE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_FALSE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_TRUE(delegate2.started());
  EXPECT_FALSE(delegate2.finished());

  EXPECT_FALSE(animation1->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());
  EXPECT_FALSE(animation2->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.finished());
  EXPECT_TRUE(delegate2.finished());

  EXPECT_TRUE(animation1->GetKeyframeEffectById(keyframe_effect_id1)
                  ->needs_push_properties());
  EXPECT_TRUE(animation2->GetKeyframeEffectById(keyframe_effect_id2)
                  ->needs_push_properties());

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
}

TEST_F(AnimationTest, AddRemoveAnimationToNonAttachedAnimation) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_));

  const double duration = 1.;
  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int filter_id = AddAnimatedFilterToAnimation(
      animation_.get(), duration, 0.1f, 0.9f, keyframe_effect_id_);
  AddOpacityTransitionToAnimation(animation_.get(), duration, start_opacity,
                                  end_opacity, false, keyframe_effect_id_);

  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);

  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_animations());
  animation_->RemoveKeyframeModelForKeyframeEffect(filter_id,
                                                   keyframe_effect_id_);
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->needs_push_properties());

  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);

  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                   ->element_animations()
                   ->HasAnyAnimationTargetingProperty(TargetProperty::FILTER));
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations()
                  ->HasAnyAnimationTargetingProperty(TargetProperty::OPACITY));
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

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
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 1u);

  time += base::TimeDelta::FromSecondsD(duration);
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

TEST_F(AnimationTest, AddRemoveAnimationCausesSetNeedsCommit) {
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  host_->AddAnimationTimeline(timeline_);
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_));

  timeline_->AttachAnimation(animation_);
  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  const int keyframe_model_id = AddOpacityTransitionToAnimation(
      animation_.get(), 1., .7f, .3f, false, keyframe_effect_id_);

  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  animation_->PauseKeyframeModelForKeyframeEffect(keyframe_model_id, 1.,
                                                  keyframe_effect_id_);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);

  animation_->RemoveKeyframeModelForKeyframeEffect(keyframe_model_id,
                                                   keyframe_effect_id_);
  EXPECT_TRUE(client_.mutators_need_commit());
  client_.set_mutators_need_commit(false);
}

// If main-thread animation switches to another layer within one frame then
// impl-thread animation must be switched as well.
TEST_F(AnimationTest, SwitchToLayer) {
  host_->AddAnimationTimeline(timeline_);
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  timeline_->AttachAnimation(animation_);
  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);

  host_->PushPropertiesTo(host_impl_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);

  EXPECT_EQ(animation_->GetKeyframeEffectById(keyframe_effect_id_),
            GetKeyframeEffectForElementId(element_id_));
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations());
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      element_id_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);
  EXPECT_EQ(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_),
            GetImplKeyframeEffectForLayerId(element_id_));
  EXPECT_TRUE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations());
  EXPECT_EQ(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      element_id_);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(false, keyframe_effect_id_);

  const ElementId new_element_id(NextTestLayerId());
  animation_->DetachElement();
  animation_->AttachElementForKeyframeEffect(new_element_id,
                                             keyframe_effect_id_);

  EXPECT_EQ(animation_->GetKeyframeEffectById(keyframe_effect_id_),
            GetKeyframeEffectForElementId(new_element_id));
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations());
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      new_element_id);
  CheckKeyframeEffectAndTimelineNeedsPushProperties(true, keyframe_effect_id_);

  host_->PushPropertiesTo(host_impl_);

  EXPECT_EQ(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_),
            GetImplKeyframeEffectForLayerId(new_element_id));
  EXPECT_TRUE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)
                  ->element_animations());
  EXPECT_EQ(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id_)->element_id(),
      new_element_id);
}

TEST_F(AnimationTest, ToString) {
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id_));
  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id_);
  EXPECT_EQ(
      base::StringPrintf("Animation{id=%d, element_id=%s, keyframe_models=[]}",
                         animation_->id(), element_id_.ToString().c_str()),
      animation_->ToString());

  animation_->AddKeyframeModelForKeyframeEffect(
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(15), 42,
                            73, TargetProperty::OPACITY),
      keyframe_effect_id_);
  EXPECT_EQ(
      base::StringPrintf("Animation{id=%d, element_id=%s, "
                         "keyframe_models=[KeyframeModel{id=42, "
                         "group=73, target_property_id=1, "
                         "run_state=WAITING_FOR_TARGET_AVAILABILITY}]}",
                         animation_->id(), element_id_.ToString().c_str()),
      animation_->ToString());

  animation_->AddKeyframeModelForKeyframeEffect(
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(18), 45,
                            76, TargetProperty::BOUNDS),
      keyframe_effect_id_);
  EXPECT_EQ(
      base::StringPrintf(
          "Animation{id=%d, element_id=%s, "
          "keyframe_models=[KeyframeModel{id=42, "
          "group=73, target_property_id=1, "
          "run_state=WAITING_FOR_TARGET_AVAILABILITY}, KeyframeModel{id=45, "
          "group=76, "
          "target_property_id=5, run_state=WAITING_FOR_TARGET_AVAILABILITY}]}",
          animation_->id(), element_id_.ToString().c_str()),
      animation_->ToString());

  KeyframeEffectId second_keyframe_effect_id =
      animation_->NextKeyframeEffectId();
  ElementId second_element_id(NextTestLayerId());
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(second_keyframe_effect_id));
  animation_->AttachElementForKeyframeEffect(second_element_id,
                                             second_keyframe_effect_id);
  animation_->AddKeyframeModelForKeyframeEffect(
      KeyframeModel::Create(std::make_unique<FakeFloatAnimationCurve>(20), 48,
                            78, TargetProperty::OPACITY),
      second_keyframe_effect_id);
  EXPECT_EQ(
      base::StringPrintf(
          "Animation{id=%d, element_id=%s, "
          "keyframe_models=[KeyframeModel{id=42, "
          "group=73, target_property_id=1, "
          "run_state=WAITING_FOR_TARGET_AVAILABILITY}, KeyframeModel{id=45, "
          "group=76, "
          "target_property_id=5, run_state=WAITING_FOR_TARGET_AVAILABILITY}]"
          ", element_id=%s, "
          "keyframe_models=[KeyframeModel{id=48, "
          "group=78, target_property_id=1, "
          "run_state=WAITING_FOR_TARGET_AVAILABILITY}]}",
          animation_->id(), element_id_.ToString().c_str(),
          second_element_id.ToString().c_str()),
      animation_->ToString());
}

TEST_F(AnimationTest,
       AddTwoKeyframeEffectsFromTheSameElementToOneAnimationTest) {
  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());

  KeyframeEffectId keyframe_effect_id1 = animation_->NextKeyframeEffectId();

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id1));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());

  KeyframeEffectId keyframe_effect_id2 = animation_->NextKeyframeEffectId();
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id2));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  timeline_->AttachAnimation(animation_);
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id1));
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id2));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id());
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);

  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id1);
  EXPECT_TRUE(animation_->element_animations(keyframe_effect_id1));
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id(),
      element_id_);
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                  ->needs_push_properties());

  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id2));
  EXPECT_NE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id(),
      element_id_);
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  const scoped_refptr<ElementAnimations> element_animations =
      host_->GetElementAnimationsForElementId(element_id_);
  EXPECT_TRUE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)));
  EXPECT_FALSE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)));

  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id2);
  EXPECT_TRUE(animation_->element_animations(keyframe_effect_id2));
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id(),
      element_id_);
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                  ->needs_push_properties());

  EXPECT_TRUE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)));

  host_->PushPropertiesTo(host_impl_);

  const scoped_refptr<ElementAnimations> element_animations_impl =
      host_impl_->GetElementAnimationsForElementId(element_id_);
  EXPECT_TRUE(element_animations_impl->HasKeyframeEffectForTesting(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id1)));
  EXPECT_TRUE(element_animations_impl->HasKeyframeEffectForTesting(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id2)));

  EXPECT_TRUE(animation_impl_->element_animations(keyframe_effect_id1));
  EXPECT_EQ(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id1)->element_id(),
      element_id_);
  EXPECT_TRUE(animation_impl_->element_animations(keyframe_effect_id2));
  EXPECT_EQ(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id2)->element_id(),
      element_id_);

  animation_->DetachElement();
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id1));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id());
  EXPECT_FALSE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)));

  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id2));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id());
  EXPECT_FALSE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)));

  EXPECT_TRUE(element_animations_impl->HasKeyframeEffectForTesting(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id1)));
  EXPECT_TRUE(element_animations_impl->HasKeyframeEffectForTesting(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id2)));

  host_->PushPropertiesTo(host_impl_);

  EXPECT_FALSE(animation_impl_->element_animations(keyframe_effect_id1));
  EXPECT_FALSE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->element_id());
  EXPECT_FALSE(element_animations_impl->HasKeyframeEffectForTesting(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id1)));
  EXPECT_FALSE(animation_impl_->element_animations(keyframe_effect_id2));
  EXPECT_FALSE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->element_id());
  EXPECT_FALSE(element_animations_impl->HasKeyframeEffectForTesting(
      animation_impl_->GetKeyframeEffectById(keyframe_effect_id2)));

  timeline_->DetachAnimation(animation_);
  EXPECT_FALSE(animation_->animation_timeline());

  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id1));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id());
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id2));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id());

  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());
}

TEST_F(AnimationTest,
       AddTwoKeyframeEffectsFromDifferentElementsToOneAnimationTest) {
  host_->AddAnimationTimeline(timeline_);

  KeyframeEffectId keyframe_effect_id1 = animation_->NextKeyframeEffectId();

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id1));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());

  KeyframeEffectId keyframe_effect_id2 = animation_->NextKeyframeEffectId();
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id2));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id1));
  EXPECT_FALSE(animation_->element_animations(keyframe_effect_id2));
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id());
  EXPECT_TRUE(timeline_->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  ElementId element1(NextTestLayerId());
  ElementId element2(NextTestLayerId());

  animation_->AttachElementForKeyframeEffect(element1, keyframe_effect_id1);
  animation_->AttachElementForKeyframeEffect(element2, keyframe_effect_id2);

  EXPECT_FALSE(animation_->animation_timeline());

  scoped_refptr<ElementAnimations> element_animations =
      host_->GetElementAnimationsForElementId(element1);
  EXPECT_FALSE(element_animations);
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->element_animations());
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id(),
      element1);
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());

  timeline_->AttachAnimation(animation_);
  EXPECT_EQ(timeline_, animation_->animation_timeline());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                  ->element_animations());
  EXPECT_EQ(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id(),
      element1);
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                  ->needs_push_properties());

  element_animations = host_->GetElementAnimationsForElementId(element1);
  EXPECT_TRUE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)));
  EXPECT_FALSE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)));

  element_animations = host_->GetElementAnimationsForElementId(element2);
  EXPECT_TRUE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)));

  animation_->DetachElement();
  EXPECT_TRUE(animation_->animation_timeline());
  EXPECT_FALSE(element_animations->HasKeyframeEffectForTesting(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->element_animations());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                  ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->element_animations());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                  ->needs_push_properties());

  // Removing animation from timeline detaches layer.
  timeline_->DetachAnimation(animation_);
  EXPECT_FALSE(animation_->animation_timeline());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->element_animations());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id1)->element_id());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                  ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->element_animations());
  EXPECT_FALSE(
      animation_->GetKeyframeEffectById(keyframe_effect_id2)->element_id());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                  ->needs_push_properties());
}

TEST_F(AnimationTest, TickingAnimationsFromTwoKeyframeEffects) {
  TestAnimationDelegate delegate1;

  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  client_impl_.RegisterElementId(element_id_, ElementListType::PENDING);
  client_impl_.RegisterElementId(element_id_, ElementListType::ACTIVE);

  KeyframeEffectId keyframe_effect_id1 = animation_->NextKeyframeEffectId();

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id1));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());

  KeyframeEffectId keyframe_effect_id2 = animation_->NextKeyframeEffectId();
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id2));
  ASSERT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  host_->AddAnimationTimeline(timeline_);

  timeline_->AttachAnimation(animation_);
  EXPECT_TRUE(timeline_->needs_push_properties());

  animation_->set_animation_delegate(&delegate1);

  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id1);
  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id2);

  const float start_opacity = .7f;
  const float end_opacity = .3f;

  const int transform_x = 10;
  const int transform_y = 20;

  const double duration = 1.;

  AddOpacityTransitionToAnimation(animation_.get(), duration, start_opacity,
                                  end_opacity, false, keyframe_effect_id1);
  AddAnimatedTransformToAnimation(animation_.get(), duration, transform_x,
                                  transform_y, keyframe_effect_id2);
  host_->PushPropertiesTo(host_impl_);
  host_impl_->ActivateAnimations(nullptr);

  EXPECT_FALSE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  base::TimeTicks time;
  time += base::TimeDelta::FromSecondsD(0.1);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.started());
  EXPECT_FALSE(delegate1.finished());

  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                   ->needs_push_properties());

  time += base::TimeDelta::FromSecondsD(duration);
  TickAnimationsTransferEvents(time, 2u);

  EXPECT_TRUE(delegate1.finished());

  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                  ->needs_push_properties());
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id2)
                  ->needs_push_properties());

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
}

TEST_F(AnimationTest, TickingState) {
  KeyframeEffectId keyframe_effect_id = animation_->NextKeyframeEffectId();

  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id));

  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(animation_);

  const int transform_x = 10;
  const int transform_y = 20;
  const double duration = 1.;
  animation_->AttachElementForKeyframeEffect(element_id_, keyframe_effect_id);
  AddAnimatedTransformToAnimation(animation_.get(), duration, transform_x,
                                  transform_y, keyframe_effect_id);
  KeyframeEffect* keyframe_effect =
      animation_->GetKeyframeEffectById(keyframe_effect_id);
  EXPECT_FALSE(keyframe_effect->is_ticking());
  client_.RegisterElementId(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(keyframe_effect->is_ticking());

  client_.UnregisterElementId(element_id_, ElementListType::ACTIVE);
  // The keyframe keeps ticking until the next call to UpdateState where it can
  // generate a finished event.
  EXPECT_TRUE(keyframe_effect->is_ticking());

  // The next call to UpdateState should remove the animation from ticking. We
  // could also assert that the finish event was generated if we also track the
  // state in the KeyframeModel correctly.
  host_->UpdateAnimationState(true, nullptr);
  EXPECT_FALSE(keyframe_effect->is_ticking());
}

TEST_F(AnimationTest, KeyframeEffectSyncToImplTest) {
  host_->AddAnimationTimeline(timeline_);
  EXPECT_TRUE(timeline_->needs_push_properties());
  timeline_->AttachAnimation(animation_);

  KeyframeEffectId keyframe_effect_id1 = animation_->NextKeyframeEffectId();
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id1));
  EXPECT_TRUE(animation_->GetKeyframeEffectById(keyframe_effect_id1));
  EXPECT_FALSE(animation_->GetKeyframeEffectById(keyframe_effect_id1)
                   ->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  animation_impl_ = timeline_impl_->GetAnimationById(animation_id_);
  EXPECT_TRUE(animation_impl_);
  EXPECT_TRUE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id1));

  EXPECT_FALSE(timeline_->needs_push_properties());

  KeyframeEffectId keyframe_effect_id2 = animation_->NextKeyframeEffectId();
  animation_->AddKeyframeEffect(
      std::make_unique<KeyframeEffect>(keyframe_effect_id2));
  EXPECT_TRUE(timeline_->needs_push_properties());

  host_->PushPropertiesTo(host_impl_);

  EXPECT_TRUE(animation_impl_->GetKeyframeEffectById(keyframe_effect_id2));
}

}  // namespace
}  // namespace cc
