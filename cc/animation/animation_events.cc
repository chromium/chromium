// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_events.h"

namespace cc {

AnimationEvent::AnimationEvent(AnimationEvent::Type type,
                               UniqueKeyframeModelId uid,
                               int group_id,
                               int target_property,
                               base::TimeTicks monotonic_time)
    : type(type),
      uid(uid),
      group_id(group_id),
      target_property(target_property),
      monotonic_time(monotonic_time),
      is_impl_only(false),
      local_time() {}

AnimationEvent::AnimationEvent(int timeline_id,
                               int animation_id,
                               std::optional<base::TimeDelta> local_time)
    : type(Type::kTimeUpdated),
      // Initializing model_id with an invalid value (0).
      // Also initializing keyframe_id with 0 which in its case is a valid
      // value. However this is safe since keyframe_id and model_id are not used
      // when routing a TIME_UPDATED event.
      uid({timeline_id, animation_id, 0}),
      group_id(),
      target_property(),
      monotonic_time(),
      is_impl_only(false),
      local_time(local_time) {}

AnimationEvent::AnimationEvent(const AnimationEvent& other) {
  type = other.type;
  uid = other.uid;
  group_id = other.group_id;
  target_property = other.target_property;
  monotonic_time = other.monotonic_time;
  is_impl_only = other.is_impl_only;
  animation_start_time = other.animation_start_time;
  if (other.curve)
    curve = other.curve->Clone();
  local_time = other.local_time;
}

AnimationEvent& AnimationEvent::operator=(const AnimationEvent& other) {
  type = other.type;
  uid = other.uid;
  group_id = other.group_id;
  target_property = other.target_property;
  monotonic_time = other.monotonic_time;
  is_impl_only = other.is_impl_only;
  animation_start_time = other.animation_start_time;
  if (other.curve)
    curve = other.curve->Clone();
  local_time = other.local_time;
  return *this;
}

AnimationEvent::~AnimationEvent() = default;

AnimationEvents::AnimationEvents() : needs_time_updated_events_(false) {}

AnimationEvents::~AnimationEvents() = default;

bool AnimationEvents::IsEmpty() const {
  return events_.empty() && !needs_time_updated_events_;
}

bool AnimationEvent::ShouldDispatchToKeyframeEffectAndModel() const {
  // TIME_UPDATED events are used to synchronize effect time between cc and
  // main thread worklet animations. Keyframe models are not involved in
  // this process.
  // is_impl_only events are not dispatched because they don't have
  // corresponding main thread components.
  return type != Type::kTimeUpdated && !is_impl_only;
}

}  // namespace cc
