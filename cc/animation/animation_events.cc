// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_events.h"

namespace cc {

AnimationEvent::AnimationEvent(AnimationEvent::Type type,
                               ElementId element_id,
                               int group_id,
                               int target_property,
                               base::TimeTicks monotonic_time)
    : type(type),
      element_id(element_id),
      worklet_animation_id(),
      group_id(group_id),
      target_property(target_property),
      monotonic_time(monotonic_time),
      is_impl_only(false),
      local_time() {}

AnimationEvent::AnimationEvent(WorkletAnimationId worklet_animation_id,
                               base::Optional<base::TimeDelta> local_time)
    : type(TIME_UPDATED),
      element_id(),
      worklet_animation_id(worklet_animation_id),
      group_id(),
      target_property(),
      monotonic_time(),
      is_impl_only(false),
      local_time(local_time) {}

AnimationEvent::AnimationEvent(const AnimationEvent& other) {
  type = other.type;
  element_id = other.element_id;
  group_id = other.group_id;
  target_property = other.target_property;
  monotonic_time = other.monotonic_time;
  is_impl_only = other.is_impl_only;
  animation_start_time = other.animation_start_time;
  if (other.curve)
    curve = other.curve->Clone();
  worklet_animation_id = other.worklet_animation_id;
  local_time = other.local_time;
}

AnimationEvent& AnimationEvent::operator=(const AnimationEvent& other) {
  type = other.type;
  element_id = other.element_id;
  group_id = other.group_id;
  target_property = other.target_property;
  monotonic_time = other.monotonic_time;
  is_impl_only = other.is_impl_only;
  animation_start_time = other.animation_start_time;
  if (other.curve)
    curve = other.curve->Clone();
  worklet_animation_id = other.worklet_animation_id;
  local_time = other.local_time;
  return *this;
}

AnimationEvent::~AnimationEvent() = default;

AnimationEvents::AnimationEvents() = default;

AnimationEvents::~AnimationEvents() = default;

bool AnimationEvents::IsEmpty() const {
  return events_.empty();
}

}  // namespace cc
