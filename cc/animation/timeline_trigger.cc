// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/timeline_trigger.h"

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/trees/property_tree.h"

namespace cc {

TimelineTrigger::TimelineTrigger(int id,
                                 scoped_refptr<AnimationTimeline> timeline,
                                 Boundaries boundaries)
    : AnimationTrigger(id), timeline_(timeline), boundaries_(boundaries) {}

TimelineTrigger::~TimelineTrigger() = default;

scoped_refptr<TimelineTrigger> TimelineTrigger::Create(
    int id,
    scoped_refptr<AnimationTimeline> timeline,
    Boundaries boundaries) {
  return base::WrapRefCounted(new TimelineTrigger(id, timeline, boundaries));
}

scoped_refptr<AnimationTrigger> TimelineTrigger::CreateImplInstance(
    AnimationHost& host_impl) const {
  scoped_refptr<AnimationTimeline> timeline_impl =
      host_impl.GetScopedRefTimelineById(timeline_.Read(*this)->id());
  CHECK(timeline_impl);
  scoped_refptr<TimelineTrigger> impl_instance =
      TimelineTrigger::Create(id(), timeline_impl, boundaries_);
  return impl_instance;
}

bool TimelineTrigger::IsTimelineTrigger() const {
  return true;
}

bool TimelineTrigger::InActivationRange(base::TimeTicks time) {
  return boundaries_.activation_start_time <= time &&
         time <= boundaries_.activation_end_time;
}

bool TimelineTrigger::InActiveRange(base::TimeTicks time) {
  return boundaries_.active_start_time <= time &&
         time <= boundaries_.active_end_time;
}

AnimationTrigger::State TimelineTrigger::ComputeState(base::TimeTicks time) {
  if (InActivationRange(time)) {
    return State::kPrimary;
  } else if (!InActiveRange(time)) {
    return State::kInverse;
  }
  return state_;
}

void TimelineTrigger::Update(const ScrollTree& scroll_tree,
                             AnimationEvents* events) {
  ScrollTimeline* scroll_timeline =
      reinterpret_cast<ScrollTimeline*>(timeline_.Read(*this).get());
  DCHECK(scroll_timeline);

  // Triggers only function based on the active tree.
  if (!scroll_timeline->IsActive(scroll_tree, /*is_active_tree=*/true)) {
    return;
  }

  std::optional<base::TimeTicks> current_time =
      scroll_timeline->CurrentTime(scroll_tree, /*is_active_tree=*/true);
  DCHECK(current_time);

  State previous_state = state_;
  State current_state = ComputeState(*current_time);

  if (previous_state == current_state) {
    // No state change, no work to do.
    return;
  }

  if (previous_state == State::kIdle && current_state == State::kInverse) {
    // "activate" must be encountered before "deactivate".
    return;
  }

  state_ = current_state;

  switch (state_) {
    case State::kPrimary:
      PerformActivate(events);
      break;
    case State::kInverse:
      PerformDeactivate(events);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace cc
