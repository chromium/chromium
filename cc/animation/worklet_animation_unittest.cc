// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include <memory>
#include <utility>
#include <vector>
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/animation_timelines_test_common.h"
#include "cc/trees/property_tree.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Unused;

namespace cc {

namespace {

class MockKeyframeEffect : public KeyframeEffect {
 public:
  explicit MockKeyframeEffect(Animation* animation)
      : KeyframeEffect(animation) {}
  MOCK_METHOD1(Tick, bool(base::TimeTicks monotonic_time));
};

class WorkletAnimationTest : public AnimationTimelinesTest {
 public:
  WorkletAnimationTest() = default;
  ~WorkletAnimationTest() override = default;

  void AttachWorkletAnimation() {
    client_.RegisterElementId(element_id_, ElementListType::ACTIVE);

    worklet_animation_ = WrapRefCounted(
        new WorkletAnimation(1, worklet_animation_id_, "test_name", 1, nullptr,
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
                       ScrollTimeline::ScrollDown,
                       /* scroll_offsets */ std::nullopt,
                       AnimationIdProvider::NextTimelineId()) {}
  MOCK_CONST_METHOD2(CurrentTime,
                     std::optional<base::TimeTicks>(const ScrollTree&, bool));
  MOCK_CONST_METHOD2(IsActive, bool(const ScrollTree&, bool));

 protected:
  ~MockScrollTimeline() override = default;
};

TEST_F(WorkletAnimationTest, NonImplInstanceDoesNotTickKeyframe) {
  scoped_refptr<WorkletAnimation> worklet_animation = WrapRefCounted(
      new WorkletAnimation(1, worklet_animation_id_, "test_name", 1, nullptr,
                           nullptr, false /* not impl instance*/));
  std::unique_ptr<MockKeyframeEffect> effect =
      std::make_unique<MockKeyframeEffect>(worklet_animation.get());
  MockKeyframeEffect* mock_effect = effect.get();
  worklet_animation->SetKeyframeEffectForTesting(std::move(effect));

  EXPECT_CALL(*mock_effect, Tick(_)).Times(0);

  MutatorOutputState::AnimationState state(worklet_animation_id_);
  state.local_times.push_back(base::Seconds(1));
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

  base::TimeDelta local_time = base::Seconds(duration / 2);
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

// Test generation of animation events by worklet animations.
TEST_F(WorkletAnimationTest, AnimationEventLocalTimeUpdate) {
  AttachWorkletAnimation();

  std::optional<base::TimeDelta> local_time = base::Seconds(1);
  MutatorOutputState::AnimationState state(worklet_animation_id_);
  state.local_times.push_back(local_time);
  worklet_animation_->SetOutputState(state);

  std::unique_ptr<MutatorEvents> mutator_events = host_->CreateEvents();
  auto* animation_events = static_cast<AnimationEvents*>(mutator_events.get());
  worklet_animation_->UpdateState(true, animation_events);

  // One event is generated as a result of update state.
  EXPECT_TRUE(animation_events->needs_time_updated_events());
  worklet_animation_->TakeTimeUpdatedEvent(animation_events);
  EXPECT_EQ(1u, animation_events->events_.size());
  AnimationEvent event = animation_events->events_[0];
  EXPECT_EQ(AnimationEvent::Type::kTimeUpdated, event.type);
  EXPECT_EQ(worklet_animation_->id(), event.uid.animation_id);
  EXPECT_EQ(local_time, event.local_time);

  // If the state is not updated no more events is generated.
  mutator_events = host_->CreateEvents();
  animation_events = static_cast<AnimationEvents*>(mutator_events.get());
  worklet_animation_->UpdateState(true, animation_events);
  EXPECT_FALSE(animation_events->needs_time_updated_events());

  // If local time is set to the same value no event is generated.
  worklet_animation_->SetOutputState(state);
  mutator_events = host_->CreateEvents();
  animation_events = static_cast<AnimationEvents*>(mutator_events.get());
  worklet_animation_->UpdateState(true, animation_events);
  EXPECT_FALSE(animation_events->needs_time_updated_events());

  // If local time is set to null value, an animation event with null local
  // time is generated.
  state.local_times.clear();
  local_time = std::nullopt;
  state.local_times.push_back(local_time);
  worklet_animation_->SetOutputState(state);
  mutator_events = host_->CreateEvents();
  animation_events = static_cast<AnimationEvents*>(mutator_events.get());
  worklet_animation_->UpdateState(true, animation_events);
  EXPECT_TRUE(animation_events->needs_time_updated_events());
  worklet_animation_->TakeTimeUpdatedEvent(animation_events);
  EXPECT_EQ(1u, animation_events->events_.size());
  EXPECT_EQ(local_time, animation_events->events_[0].local_time);
}

TEST_F(WorkletAnimationTest, CurrentTimeCorrectlyUsesScrollTimeline) {
  auto scroll_timeline = base::WrapRefCounted(new MockScrollTimeline());
  EXPECT_CALL(*scroll_timeline, IsActive(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*scroll_timeline, CurrentTime(_, _))
      .WillRepeatedly(Return((base::TimeTicks() + base::Milliseconds(1234))));
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", 1, nullptr, nullptr);
  host_->AddAnimationTimeline(scroll_timeline);
  scroll_timeline->AttachAnimation(worklet_animation);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks::Now(),
                                      scroll_tree, true);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(1234, input->added_and_updated_animations[0].current_time);
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromRegularTimelineIsOffsetByStartTime) {
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", 1, nullptr, nullptr);

  worklet_animation->AttachElement(element_id_);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(worklet_animation);

  base::TimeTicks first_ticks = base::TimeTicks() + base::Milliseconds(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::Milliseconds(111 + 123.4);
  base::TimeTicks third_ticks =
      base::TimeTicks() + base::Milliseconds(111 + 246.8);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), first_ticks, scroll_tree,
                                      true);
  // First state request sets the start time and thus current time should be 0.
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(0, input->added_and_updated_animations[0].current_time);
  state = std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), second_ticks, scroll_tree,
                                      true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(123.4, input->updated_animations[0].current_time);
  // Should always offset from start time.
  state = std::make_unique<MutatorInputState>();
  worklet_animation->UpdateInputState(state.get(), third_ticks, scroll_tree,
                                      true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(246.8, input->updated_animations[0].current_time);
}

// Verifies correctness of current time when playback rate is set on
// initializing the animation and while the animation is playing.
TEST_F(WorkletAnimationTest, DocumentTimelineSetPlaybackRate) {
  const double playback_rate_double = 2;
  const double playback_rate_half = 0.5;
  scoped_refptr<WorkletAnimation> worklet_animation =
      WorkletAnimation::Create(worklet_animation_id_, "test_name",
                               playback_rate_double, nullptr, nullptr);

  worklet_animation->AttachElement(element_id_);
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachAnimation(worklet_animation);

  base::TimeTicks first_ticks = base::TimeTicks() + base::Milliseconds(111);
  base::TimeTicks second_ticks =
      base::TimeTicks() + base::Milliseconds(111 + 123.4);
  base::TimeTicks third_ticks =
      base::TimeTicks() + base::Milliseconds(111 + 123.4 + 200.0);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  // Start the animation.
  worklet_animation->UpdateInputState(state.get(), first_ticks, scroll_tree,
                                      true);
  state = std::make_unique<MutatorInputState>();

  // Play until second_ticks.
  worklet_animation->UpdateInputState(state.get(), second_ticks, scroll_tree,
                                      true);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.worklet_id);

  // Verify that the current time is updated twice faster than the timeline
  // time.
  EXPECT_EQ(123.4 * playback_rate_double,
            input->updated_animations[0].current_time);

  // Update the playback rate.
  worklet_animation->SetPlaybackRateForTesting(playback_rate_half);
  state = std::make_unique<MutatorInputState>();

  // Play until third_ticks.
  worklet_animation->UpdateInputState(state.get(), third_ticks, scroll_tree,
                                      true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);

  // Verify that the current time is updated half as fast as the timeline time.
  EXPECT_EQ(123.4 * playback_rate_double + 200.0 * playback_rate_half,
            input->updated_animations[0].current_time);
}

// Verifies correctness of current time when playback rate is set on
// initializing the scroll-linked animation and while the animation is playing.
TEST_F(WorkletAnimationTest, ScrollTimelineSetPlaybackRate) {
  const double playback_rate_double = 2;
  const double playback_rate_half = 0.5;
  auto scroll_timeline = base::WrapRefCounted(new MockScrollTimeline());

  scoped_refptr<WorkletAnimation> worklet_animation =
      WorkletAnimation::Create(worklet_animation_id_, "test_name",
                               playback_rate_double, nullptr, nullptr);
  host_->AddAnimationTimeline(scroll_timeline);
  scroll_timeline->AttachAnimation(worklet_animation);
  const MockScrollTimeline* mock_timeline =
      static_cast<const MockScrollTimeline*>(
          worklet_animation->animation_timeline());

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();
  // Start the animation.
  EXPECT_CALL(*mock_timeline, IsActive(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_timeline, CurrentTime(_, _))
      .WillRepeatedly(Return(base::TimeTicks() + base::Milliseconds(50)));
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks(),
                                      scroll_tree, true);
  Mock::VerifyAndClearExpectations(&mock_timeline);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.worklet_id);

  // Verify that the current time is updated twice faster than the timeline
  // time.
  EXPECT_EQ(50 * playback_rate_double,
            input->added_and_updated_animations[0].current_time);

  // Update the playback rate.
  worklet_animation->SetPlaybackRateForTesting(playback_rate_half);
  state = std::make_unique<MutatorInputState>();

  // Continue playing the animation.
  EXPECT_CALL(*mock_timeline, IsActive(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_timeline, CurrentTime(_, _))
      .WillRepeatedly(Return(base::TimeTicks() + base::Milliseconds(100)));
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks(),
                                      scroll_tree, true);
  Mock::VerifyAndClearExpectations(&mock_timeline);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);

  // Verify that the current time is updated half as fast as the timeline time.
  EXPECT_EQ(50 * playback_rate_double + 50 * playback_rate_half,
            input->updated_animations[0].current_time);
}

// Verifies correcteness of worklet animation current time when inactive
// timeline becomes active and then inactive again.
TEST_F(WorkletAnimationTest, InactiveScrollTimeline) {
  auto scroll_timeline = base::WrapRefCounted(new MockScrollTimeline());

  scoped_refptr<WorkletAnimation> worklet_animation =
      WorkletAnimation::Create(worklet_animation_id_, "test_name",
                               /*playback_rate*/ 1, nullptr, nullptr);

  host_->AddAnimationTimeline(scroll_timeline);
  scroll_timeline->AttachAnimation(worklet_animation);
  const MockScrollTimeline* mock_timeline =
      static_cast<const MockScrollTimeline*>(
          worklet_animation->animation_timeline());
  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();

  // Start the animation with inactive timeline.
  EXPECT_CALL(*mock_timeline, IsActive(_, _)).WillRepeatedly(Return(false));
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks(),
                                      scroll_tree, true);
  Mock::VerifyAndClearExpectations(&mock_timeline);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_FALSE(input);
  state = std::make_unique<MutatorInputState>();

  // Now the timeline is active.
  EXPECT_CALL(*mock_timeline, IsActive(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_timeline, CurrentTime(_, _))
      .WillRepeatedly(Return(base::TimeTicks() + base::Milliseconds(100)));
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks(),
                                      scroll_tree, true);
  Mock::VerifyAndClearExpectations(&mock_timeline);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  // Verify that the current time is updated when the timeline becomes newly
  // active.
  EXPECT_EQ(100, input->added_and_updated_animations[0].current_time);
  state = std::make_unique<MutatorInputState>();

  // Now the timeline is inactive.
  EXPECT_CALL(*mock_timeline, IsActive(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_timeline, CurrentTime(_, _))
      .WillRepeatedly(Return(base::TimeTicks() + base::Milliseconds(200)));
  worklet_animation->UpdateInputState(state.get(), base::TimeTicks(),
                                      scroll_tree, true);
  Mock::VerifyAndClearExpectations(&mock_timeline);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  // No update of the input state when the timeline is inactive.
  EXPECT_FALSE(input);
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
      state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 1u);
  EXPECT_EQ("test_name", input->added_and_updated_animations[0].name);
  EXPECT_EQ(input->updated_animations.size(), 0u);
  EXPECT_EQ(input->removed_animations.size(), 0u);

  // The state of WorkletAnimation is updated to RUNNING after calling
  // UpdateInputState above.
  state = std::make_unique<MutatorInputState>();
  time += base::Seconds(0.1);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(input->updated_animations.size(), 1u);
  EXPECT_EQ(input->removed_animations.size(), 0u);

  // Operating on individual KeyframeModel doesn't affect the state of
  // WorkletAnimation.
  keyframe_model->SetRunState(KeyframeModel::FINISHED, time);
  state = std::make_unique<MutatorInputState>();
  time += base::Seconds(0.1);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 0u);
  EXPECT_EQ(input->updated_animations.size(), 1u);
  EXPECT_EQ(input->removed_animations.size(), 0u);

  // WorkletAnimation sets state to REMOVED when JavaScript fires cancel() which
  // leads to RemoveKeyframeModel.
  worklet_animation_->RemoveKeyframeModel(keyframe_model_id);
  worklet_animation_->UpdateState(true, nullptr);
  state = std::make_unique<MutatorInputState>();
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
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
      state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 1u);
  EXPECT_EQ(input->updated_animations.size(), 0u);

  state = std::make_unique<MutatorInputState>();
  // No update on the input state if input time stays the same.
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_FALSE(input);

  state = std::make_unique<MutatorInputState>();
  // Different input time causes the input state to be updated.
  time += base::Seconds(0.1);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->updated_animations.size(), 1u);

  state = std::make_unique<MutatorInputState>();
  // Input state gets updated when the worklet animation is to be removed even
  // the input time doesn't change.
  worklet_animation_->RemoveKeyframeModel(keyframe_model_id);
  worklet_animation_->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->updated_animations.size(), 0u);
  EXPECT_EQ(input->removed_animations.size(), 1u);
}

std::optional<base::TimeTicks> FakeIncreasingScrollTimelineTime(Unused,
                                                                Unused) {
  static base::TimeTicks current_time;
  current_time += base::Seconds(0.1);
  return current_time;
}

// This test verifies that worklet animation gets skipped properly if a pending
// mutation cycle is holding a lock on the worklet.
TEST_F(WorkletAnimationTest, SkipLockedAnimations) {
  auto scroll_timeline = base::WrapRefCounted(new MockScrollTimeline());
  EXPECT_CALL(*scroll_timeline, IsActive(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(*scroll_timeline, CurrentTime(_, _))
      .WillRepeatedly(Invoke(FakeIncreasingScrollTimelineTime));
  scoped_refptr<WorkletAnimation> worklet_animation = WorkletAnimation::Create(
      worklet_animation_id_, "test_name", 1, nullptr, nullptr);
  host_->AddAnimationTimeline(scroll_timeline);
  scroll_timeline->AttachAnimation(worklet_animation);

  ScrollTree scroll_tree;
  std::unique_ptr<MutatorInputState> state =
      std::make_unique<MutatorInputState>();

  base::TimeTicks time;
  worklet_animation->UpdateInputState(state.get(), time, scroll_tree, true);
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->added_and_updated_animations.size(), 1u);
  EXPECT_EQ(input->updated_animations.size(), 0u);

  state = std::make_unique<MutatorInputState>();
  // Different scroll time causes the input state to be updated.
  worklet_animation->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->updated_animations.size(), 1u);

  state = std::make_unique<MutatorInputState>();
  // Different scroll time causes the input state to be updated. Pending
  // mutation will grab a lock.
  worklet_animation->UpdateInputState(state.get(), time, scroll_tree, false);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->updated_animations.size(), 1u);

  state = std::make_unique<MutatorInputState>();
  // Pending lock has not been released.
  worklet_animation->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_FALSE(input);

  worklet_animation->ReleasePendingTreeLock();

  state = std::make_unique<MutatorInputState>();
  // Pending lock has been released.
  worklet_animation->UpdateInputState(state.get(), time, scroll_tree, true);
  input = state->TakeWorkletState(worklet_animation_id_.worklet_id);
  EXPECT_EQ(input->updated_animations.size(), 1u);
}

}  // namespace

}  // namespace cc
