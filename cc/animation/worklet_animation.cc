// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include <utility>
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/trees/animation_effect_timings.h"
#include "cc/trees/animation_options.h"

namespace cc {

WorkletAnimation::WorkletAnimation(
    int cc_animation_id,
    WorkletAnimationId worklet_animation_id,
    const std::string& name,
    double playback_rate_value,
    std::unique_ptr<AnimationOptions> options,
    std::unique_ptr<AnimationEffectTimings> effect_timings,
    bool is_controlling_instance)
    : Animation(cc_animation_id),
      worklet_animation_id_(worklet_animation_id),
      name_(name),
      playback_rate_(playback_rate_value),
      options_(std::move(options)),
      effect_timings_(std::move(effect_timings)),
      is_impl_instance_(is_controlling_instance) {}

WorkletAnimation::~WorkletAnimation() = default;

scoped_refptr<WorkletAnimation> WorkletAnimation::Create(
    WorkletAnimationId worklet_animation_id,
    const std::string& name,
    double playback_rate_value,
    std::unique_ptr<AnimationOptions> options,
    std::unique_ptr<AnimationEffectTimings> effect_timings) {
  return WrapRefCounted(new WorkletAnimation(
      AnimationIdProvider::NextAnimationId(), worklet_animation_id, name,
      playback_rate_value, std::move(options), std::move(effect_timings),
      false));
}

scoped_refptr<Animation> WorkletAnimation::CreateImplInstance() const {
  return WrapRefCounted(
      new WorkletAnimation(id(), worklet_animation_id_, name(), playback_rate(),
                           CloneOptions(), CloneEffectTimings(), true));
}

void WorkletAnimation::PushPropertiesTo(Animation* animation_impl) {
  Animation::PushPropertiesTo(animation_impl);
  WorkletAnimation* worklet_animation_impl = ToWorkletAnimation(animation_impl);
  worklet_animation_impl->SetPlaybackRate(playback_rate());
}

bool WorkletAnimation::Tick(base::TimeTicks monotonic_time) {
  // Do not tick worklet animations on main thread as we will tick them on the
  // compositor and the tick is more expensive than regular animations.
  if (!is_impl_instance_) {
    return false;
  }
  if (!local_time_.Read(*this).has_value()) {
    return false;
  }
  // As the output of a WorkletAnimation is driven by a script-provided local
  // time, we don't want the underlying effect to participate in the normal
  // animations lifecycle. To avoid this we pause the underlying keyframe effect
  // at the local time obtained from the user script - essentially turning each
  // call to |WorkletAnimation::Tick| into a seek in the effect.
  keyframe_effect()->Pause(base::TimeTicks() + local_time_.Read(*this).value());
  keyframe_effect()->Tick(base::TimeTicks());
  return true;
}

void WorkletAnimation::UpdateState(bool start_ready_animations,
                                   AnimationEvents* events) {
  Animation::UpdateState(start_ready_animations, events);
  if (last_synced_local_time_.Read(*this) != local_time_.Read(*this))
    events->set_needs_time_updated_events(true);
}

void WorkletAnimation::TakeTimeUpdatedEvent(AnimationEvents* events) {
  DCHECK(events->needs_time_updated_events());
  if (last_synced_local_time_.Read(*this) != local_time_.Read(*this)) {
    AnimationEvent event(animation_timeline()->id(), id_,
                         local_time_.Read(*this));
    events->events_.push_back(event);
    last_synced_local_time_.Write(*this) = local_time_.Read(*this);
  }
}

void WorkletAnimation::UpdateInputState(MutatorInputState* input_state,
                                        base::TimeTicks monotonic_time,
                                        const ScrollTree& scroll_tree,
                                        bool is_active_tree) {
  bool is_timeline_active = IsTimelineActive(scroll_tree, is_active_tree);
  // Record the monotonic time to be the start time first time state is
  // generated. This time is used as the origin for computing the current time.
  // The start time of scroll-linked animations is always initialized to zero.
  // See: https://github.com/w3c/csswg-drafts/issues/2075
  // To stay consistent with blink::WorkletAnimation, record start time only
  // when the timeline becomes active.
  if (!start_time_.Read(*this).has_value() && is_timeline_active)
    start_time_.Write(*this) = animation_timeline()->IsScrollTimeline()
                                   ? base::TimeTicks()
                                   : monotonic_time;

  if (is_active_tree && has_pending_tree_lock_.Read(*this))
    return;

  // Skip running worklet animations with unchanged input time and reuse
  // their value from the previous animation call.
  if (!NeedsUpdate(monotonic_time, scroll_tree, is_active_tree))
    return;

  DCHECK(is_timeline_active || state_.Read(*this) == State::kRemoved);

  // TODO(crbug.com/40651525): Initialize current_time to null if the
  // timeline is inactive. It might be inactive here when state is
  // State::REMOVED.
  std::optional<base::TimeDelta> current_time =
      CurrentTime(monotonic_time, scroll_tree, is_active_tree);

  // When the timeline is inactive (only the case with scroll timelines), the
  // animation holds its last current time and last current output. This
  // means we don't need to produce any new input state. See also:
  // https://drafts.csswg.org/web-animations/#responding-to-a-newly-inactive-timeline
  if (!is_timeline_active)
    current_time = last_current_time_.Read(*this);

  // TODO(crbug.com/40651525): Do not early exit if state is
  // State::REMOVED. The animation must be removed in this case.
  if (!current_time)
    return;
  last_current_time_.Write(*this) = current_time;

  // Prevent active tree mutations from queuing up until pending tree is
  // activated to preserve flow of time for scroll timelines.
  has_pending_tree_lock_.Write(*this) =
      !is_active_tree && animation_timeline()->IsScrollTimeline();

  switch (state_.Read(*this)) {
    case State::kPending:
      input_state->Add({worklet_animation_id(), name(),
                        current_time->InMillisecondsF(), CloneOptions(),
                        CloneEffectTimings()});
      state_.Write(*this) = State::kRunning;
      break;
    case State::kRunning:
      // TODO(jortaylo): EffectTimings need to be sent to the worklet during
      // updates, otherwise the timing info will become outdated.
      // https://crbug.com/915344.
      input_state->Update(
          {worklet_animation_id(), current_time->InMillisecondsF()});
      break;
    case State::kRemoved:
      input_state->Remove(worklet_animation_id());
      break;
  }
}

void WorkletAnimation::SetOutputState(
    const MutatorOutputState::AnimationState& state) {
  DCHECK_EQ(state.local_times.size(), 1u);
  local_time_.Write(*this) = state.local_times[0];
}

void WorkletAnimation::SetPlaybackRate(double rate) {
  if (rate == playback_rate())
    return;

  // Setting playback rate is rejected in the blink side if playback_rate_ is
  // zero.
  DCHECK(playback_rate());

  if (start_time_.Read(*this) && last_current_time_.Read(*this)) {
    // Update startTime in order to maintain previous currentTime and,
    // as a result, prevent the animation from jumping.
    base::TimeDelta current_time = last_current_time_.Read(*this).value();
    start_time_.Write(*this) = start_time_.Read(*this).value() +
                               current_time / playback_rate() -
                               current_time / rate;
  }
  playback_rate_.Write(*this) = rate;
}

void WorkletAnimation::UpdatePlaybackRate(double rate) {
  if (rate == playback_rate())
    return;
  playback_rate_.Write(*this) = rate;
  SetNeedsPushProperties();
}

void WorkletAnimation::ReleasePendingTreeLock() {
  has_pending_tree_lock_.Write(*this) = false;
}

std::optional<base::TimeDelta> WorkletAnimation::CurrentTime(
    base::TimeTicks monotonic_time,
    const ScrollTree& scroll_tree,
    bool is_active_tree) {
  DCHECK(IsTimelineActive(scroll_tree, is_active_tree));
  base::TimeTicks timeline_time;
  if (animation_timeline()->IsScrollTimeline()) {
    std::optional<base::TimeTicks> scroll_monotonic_time =
        ToScrollTimeline(animation_timeline())
            ->CurrentTime(scroll_tree, is_active_tree);
    if (!scroll_monotonic_time)
      return std::nullopt;
    timeline_time = scroll_monotonic_time.value();
  } else {
    timeline_time = monotonic_time;
  }
  return (timeline_time - start_time_.Read(*this).value()) * playback_rate();
}

bool WorkletAnimation::NeedsUpdate(base::TimeTicks monotonic_time,
                                   const ScrollTree& scroll_tree,
                                   bool is_active_tree) {
  if (state_.Read(*this) == State::kRemoved) {
    return true;
  }

  // When the timeline is inactive we apply the last current time to the
  // animation.
  if (!IsTimelineActive(scroll_tree, is_active_tree))
    return false;

  std::optional<base::TimeDelta> current_time =
      CurrentTime(monotonic_time, scroll_tree, is_active_tree);
  bool needs_update = last_current_time_.Read(*this) != current_time;
  return needs_update;
}

bool WorkletAnimation::IsTimelineActive(const ScrollTree& scroll_tree,
                                        bool is_active_tree) const {
  if (!animation_timeline()->IsScrollTimeline())
    return true;

  return ToScrollTimeline(animation_timeline())
      ->IsActive(scroll_tree, is_active_tree);
}

void WorkletAnimation::RemoveKeyframeModel(int keyframe_model_id) {
  state_.Write(*this) = State::kRemoved;
  Animation::RemoveKeyframeModel(keyframe_model_id);
}

bool WorkletAnimation::IsWorkletAnimation() const {
  return true;
}

}  // namespace cc
