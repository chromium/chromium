// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/trees/animation_options.h"

namespace cc {

WorkletAnimation::WorkletAnimation(
    int cc_animation_id,
    WorkletAnimationId worklet_animation_id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline,
    std::unique_ptr<AnimationOptions> options,
    bool is_controlling_instance)
    : WorkletAnimation(cc_animation_id,
                       worklet_animation_id,
                       name,
                       std::move(scroll_timeline),
                       std::move(options),
                       is_controlling_instance,
                       nullptr) {}

WorkletAnimation::WorkletAnimation(
    int cc_animation_id,
    WorkletAnimationId worklet_animation_id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline,
    std::unique_ptr<AnimationOptions> options,
    bool is_controlling_instance,
    std::unique_ptr<KeyframeEffect> effect)
    : SingleKeyframeEffectAnimation(cc_animation_id, std::move(effect)),
      worklet_animation_id_(worklet_animation_id),
      name_(name),
      scroll_timeline_(std::move(scroll_timeline)),
      options_(std::move(options)),
      local_time_(base::nullopt),
      start_time_(base::nullopt),
      last_current_time_(base::nullopt),
      state_(State::PENDING),
      is_impl_instance_(is_controlling_instance) {}

WorkletAnimation::~WorkletAnimation() = default;

scoped_refptr<WorkletAnimation> WorkletAnimation::Create(
    WorkletAnimationId worklet_animation_id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline,
    std::unique_ptr<AnimationOptions> options) {
  return WrapRefCounted(new WorkletAnimation(
      AnimationIdProvider::NextAnimationId(), worklet_animation_id, name,
      std::move(scroll_timeline), std::move(options), false));
}

scoped_refptr<Animation> WorkletAnimation::CreateImplInstance() const {
  std::unique_ptr<ScrollTimeline> impl_timeline;
  if (scroll_timeline_)
    impl_timeline = scroll_timeline_->CreateImplInstance();

  return WrapRefCounted(new WorkletAnimation(id(), worklet_animation_id_,
                                             name(), std::move(impl_timeline),
                                             CloneOptions(), true));
}

void WorkletAnimation::PushPropertiesTo(Animation* animation_impl) {
  Animation::PushPropertiesTo(animation_impl);
  WorkletAnimation* worklet_animation_impl = ToWorkletAnimation(animation_impl);
  if (scroll_timeline_) {
    scroll_timeline_->PushPropertiesTo(
        worklet_animation_impl->scroll_timeline_.get());
  }
}

void WorkletAnimation::Tick(base::TimeTicks monotonic_time) {
  // Do not tick worklet animations on main thread. This should be removed if we
  // skip ticking all animations on main thread in http://crbug.com/762717.
  if (!is_impl_instance_)
    return;
  if (!local_time_.has_value())
    return;
  // As the output of a WorkletAnimation is driven by a script-provided local
  // time, we don't want the underlying effect to participate in the normal
  // animations lifecycle. To avoid this we pause the underlying keyframe effect
  // at the local time obtained from the user script - essentially turning each
  // call to |WorkletAnimation::Tick| into a seek in the effect.
  keyframe_effect()->Pause(local_time_.value());
  keyframe_effect()->Tick(monotonic_time);
}

void WorkletAnimation::UpdateInputState(MutatorInputState* input_state,
                                        base::TimeTicks monotonic_time,
                                        const ScrollTree& scroll_tree,
                                        bool is_active_tree) {
  // Record the monotonic time to be the start time first time state is
  // generated. This time is used as the origin for computing the current time.
  if (!start_time_.has_value())
    start_time_ = monotonic_time;

  // Skip running worklet animations with unchanged input time and reuse
  // their value from the previous animation call.
  if (!NeedsUpdate(monotonic_time, scroll_tree, is_active_tree))
    return;

  double current_time =
      CurrentTime(monotonic_time, scroll_tree, is_active_tree);
  last_current_time_ = current_time;

  switch (state_) {
    case State::PENDING:
      // TODO(yigu): cc side WorkletAnimation is only capable of handling single
      // keyframe effect at the moment. We should pass in the number of effects
      // once Worklet Group Effect is fully implemented in cc.
      // https://crbug.com/767043.
      input_state->Add({worklet_animation_id(), name(), current_time,
                        CloneOptions(), 1 /* num_effects */});
      state_ = State::RUNNING;
      break;
    case State::RUNNING:
      input_state->Update({worklet_animation_id(), current_time});
      break;
    case State::REMOVED:
      input_state->Remove(worklet_animation_id());
      break;
  }
}

void WorkletAnimation::SetOutputState(
    const MutatorOutputState::AnimationState& state) {
  // TODO(yigu): cc side WorkletAnimation is only capable of handling single
  // keyframe effect at the moment. https://crbug.com/767043.
  DCHECK_EQ(state.local_times.size(), 1u);
  local_time_ = state.local_times[0];
}

// TODO(crbug.com/780151): Multiply the result by the play back rate.
double WorkletAnimation::CurrentTime(base::TimeTicks monotonic_time,
                                     const ScrollTree& scroll_tree,
                                     bool is_active_tree) {
  // Note that we have intentionally decided not to offset the scroll timeline
  // by the start time. See: https://github.com/w3c/csswg-drafts/issues/2075
  if (scroll_timeline_)
    return scroll_timeline_->CurrentTime(scroll_tree, is_active_tree);
  return (monotonic_time - start_time_.value()).InMillisecondsF();
}

bool WorkletAnimation::NeedsUpdate(base::TimeTicks monotonic_time,
                                   const ScrollTree& scroll_tree,
                                   bool is_active_tree) {
  // If we don't have a start time it means that an update was never sent to
  // the worklet therefore we need one.
  if (!scroll_timeline_ && !start_time_.has_value())
    return true;

  DCHECK(state_ == State::PENDING || last_current_time_.has_value());
  if (state_ == State::REMOVED)
    return true;

  double current_time =
      CurrentTime(monotonic_time, scroll_tree, is_active_tree);
  bool needs_update = last_current_time_ != current_time;
  return needs_update;
}

void WorkletAnimation::UpdateScrollTimeline(
    base::Optional<ElementId> scroller_id,
    base::Optional<double> start_scroll_offset,
    base::Optional<double> end_scroll_offset) {
  // Calling this method implies that we are a ScrollTimeline based animation,
  // so the below call is done unchecked.
  scroll_timeline_->SetScrollerId(scroller_id);
  scroll_timeline_->UpdateStartAndEndScrollOffsets(start_scroll_offset,
                                                   end_scroll_offset);
  SetNeedsPushProperties();
}

void WorkletAnimation::PromoteScrollTimelinePendingToActive() {
  if (scroll_timeline_)
    scroll_timeline_->PromoteScrollTimelinePendingToActive();
}

void WorkletAnimation::RemoveKeyframeModel(int keyframe_model_id) {
  state_ = State::REMOVED;
  SingleKeyframeEffectAnimation::RemoveKeyframeModel(keyframe_model_id);
}

bool WorkletAnimation::IsWorkletAnimation() const {
  return true;
}

}  // namespace cc
