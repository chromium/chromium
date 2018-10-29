// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "cc/trees/property_tree.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace cc {

namespace {

class MockKeyframeEffect : public KeyframeEffect {
 public:
  MockKeyframeEffect() : KeyframeEffect(0) {}
  MOCK_METHOD1(Tick, void(base::TimeTicks monotonic_time));
};

class WorkletAnimationTest : public AnimationTimelinesTest {
 public:
  WorkletAnimationTest() = default;
  ~WorkletAnimationTest() override = default;

  void AttachWorkletAnimation() {
    client_.RegisterElement(element_id_, ElementListType::ACTIVE);

    worklet_animation_ = WrapRefCounted(
        new WorkletAnimation(1, worklet_animation_id_, "test_name", nullptr,
                             nullptr, true /* controlling instance*/));
    worklet_animation_->AttachElement(element_id_);
    host_->AddAnimationTimeline(timeline_);
    timeline_->AttachAnimation(worklet_animation_);
  }

  scoped_refptr<WorkletAnimation> worklet_animation_;
  WorkletAnimationId worklet_animation_id_{11, 12};
};

class MockScrollTimeline : public ScrollTimeline {
 public:
  MockScrollTimeline()
      : ScrollTimeline(ElementId(),
                       ScrollTimeline::Vertical,
                       base::nullopt,
                       base::nullopt,
                       0) {}
  MOCK_CONST_METHOD2(CurrentTime, double(const ScrollTree&, bool));
};

TEST_F(WorkletAnimationTest, NonImplInstanceDoesNotTickKeyframe) {
  std::unique_ptr<MockKeyframeEffect> effect =
      std::make_unique<MockKeyframeEffect>();
  MockKeyframeEffect* mock_effect = effect.get();

  scoped_refptr<WorkletAnimation> worklet_animation =
      WrapRefCounted(new WorkletAnimation(
          1, worklet_animation_id_, "test_name", nullptr, nullptr,
          false /* not impl instance*/, std::move(effect)));

  EXPECT_CALL(*mock_effect, Tick(_)).Times(0);

  MutatorOutputState::AnimationState state(worklet_animation_id_);
  state.local_times.push_back(base::TimeDelta::FromSecondsD(1));
  worklet_animation->SetOutputState(state);
  worklet_animation->Tick(base::TimeTicks());
}

TEST_F(WorkletAnimationTest, LocalTimeIsUsedWhenTicking) {
  AttachWorkletAnimation();

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  const float expected_opacity =
      start_opacity + (end_opacity - start_opacity) / 2;
  AddOpacityTransitionToAnimation(worklet_animation_.get(), duration,
                                  start_opacity, end_opacity, true);

  KeyframeModel* keyframe_model =
      worklet_animation_->GetKeyframeModel(TargetProperty::OPACITY);
  // Impl side animation don't need synchronized start time.
  keyframe_model->set_needs_synchronized_start_time(false);

  base::TimeDelta local_time = base::TimeDelta::FromSecondsD(duration / 2);
  MutatorOutputState::AnimationState state(worklet_animation_id_);
  state.local_times.push_back(local_time);
  worklet_animation_->SetOutputState(state);

  worklet_animation_->Tick(base::TimeTicks());

  TestLayer* layer =
      client_.FindTestLayer(element_id_, ElementListType::ACTIVE);
  EXPECT_TRUE(layer->is_property_mutated(TargetProperty::OPACITY));
  client_.ExpectOpacityPropertyMutated(element_id_, ElementListType::ACTIVE,
                                       expected_opacity);
}

TEST_F(WorkletAnimationTest, CurrentTimeCorrectlyUsesScrollTimeline) {
  auto scroll_timeline = std::make_unique<MockScrollTimeline>();
  EXPECT_CALL(*scroll_timeline, CurrentTime(_, _)).WillRepeatedly(Return(1234));
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", std::move(scroll_timeline), nullptr);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks::Now(),
                                      scroll_tree, true);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(1234, input->added_and_updated_animations[0].current_time);
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromDocumentTimelineIsOffsetByStartTime) {
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", nullptr, nullptr);

  base::TimeTicks first_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 123.4);
  base::TimeTicks third_ticks =
      base::TimeTicks() + base::TimeDelta::FromMillisecondsD(111 + 246.8);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), first_ticks, scroll_tree,
                                      true);
  // First state request sets the start time and thus current time should be 0.
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(0, input->added_and_updated_animations[0].current_time);
  state.reset(new MutatorInputState);
  worklet_animation->UpdateInputState(state.get(), second_ticks, scroll_tree,
                                      true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(123.4, input->updated_animations[0].current_time);
  // Should always offset from start time.
  state.reset(new MutatorInputState());
  worklet_animation->UpdateInputState(state.get(), third_ticks, scroll_tree,
                                      true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(246.8, input->updated_animations[0].current_time);
}

// This test verifies that worklet animation state is properly updated.
TEST_F(WorkletAnimationTest, UpdateInputStateProducesCorrectState) {
  AttachWorkletAnimation();

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  int keyframe_model_id = AddOpacityTransitionToAnimation(
      worklet_animation_.get(), duration, start_opacity, end_opacity, true);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();

  KeyframeModel* keyframe_model =
      worklet_animation_->GetKeyframeModel(TargetProperty::OPACITY);
  ASSERT_TRUE(keyframe_model);

  base::TimeTicks time;
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 1u);
  EXPECT_EQ("test_name", input->added_and_updated_animations[0].name);
  EXPECT_EQ(input->updated_animations.size(), 0u);
  EXPECT_EQ(input->removed_animations.size(), 0u);

  // The state of WorkletAnimation is updated to RUNNING after calling
  // UpdateInputState above.
  state.reset(new MutatorInputState());
  time += base::TimeDelta::FromSecondsD(0.1);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(input->updated_animations.size(), 1u);
  EXPECT_EQ(input->removed_animations.size(), 0u);

  // Operating on individual KeyframeModel doesn't affect the state of
  // WorkletAnimation.
  keyframe_model->SetRunState(KeyframeModel::FINISHED, time);
  state.reset(new MutatorInputState());
  time += base::TimeDelta::FromSecondsD(0.1);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(input->updated_animations.size(), 1u);
  EXPECT_EQ(input->removed_animations.size(), 0u);

  // WorkletAnimation sets state to REMOVED when JavaScript fires cancel() which
  // leads to RemoveKeyframeModel.
  worklet_animation_->RemoveKeyframeModel(keyframe_model_id);
  worklet_animation_->UpdateState(true, nullptr);
  state.reset(new MutatorInputState());
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(input->updated_animations.size(), 0u);
  EXPECT_EQ(input->removed_animations.size(), 1u);
  EXPECT_EQ(input->removed_animations[0], worklet_animation_id_);
}

// This test verifies that worklet animation gets skipped properly.
TEST_F(WorkletAnimationTest, SkipUnchangedAnimations) {
  AttachWorkletAnimation();

  const float start_opacity = .7f;
  const float end_opacity = .3f;
  const double duration = 1.;

  int keyframe_model_id = AddOpacityTransitionToAnimation(
      worklet_animation_.get(), duration, start_opacity, end_opacity, true);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();

  base::TimeTicks time;
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 1u);
  EXPECT_EQ(input->updated_animations.size(), 0u);

  state.reset(new MutatorInputState());
  // No update on the input state if input time stays the same.
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_FALSE(input);

  state.reset(new MutatorInputState());
  // Different input time causes the input state to be updated.
  time += base::TimeDelta::FromSecondsD(0.1);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->updated_animations.size(), 1u);

  state.reset(new MutatorInputState());
  // Input state gets updated when the worklet animation is to be removed even
  // the input time doesn't change.
  worklet_animation_->RemoveKeyframeModel(keyframe_model_id);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.scope_id);
  EXPECT_EQ(input->updated_animations.size(), 0u);
  EXPECT_EQ(input->removed_animations.size(), 1u);
}

}  // namespace

}  // namespace cc
