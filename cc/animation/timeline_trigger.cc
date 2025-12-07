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
                                 scoped_refptr<AnimationTimeline> timeline)
    : AnimationTrigger(id), timeline_(timeline) {}

TimelineTrigger::~TimelineTrigger() = default;

scoped_refptr<TimelineTrigger> TimelineTrigger::Create(
    int id,
    scoped_refptr<AnimationTimeline> timeline) {
  return base::WrapRefCounted(new TimelineTrigger(id, timeline));
}

scoped_refptr<AnimationTrigger> TimelineTrigger::CreateImplInstance(
    AnimationHost& host_impl) const {
  scoped_refptr<AnimationTimeline> timeline_impl =
      host_impl.GetScopedRefTimelineById(timeline_.Read(*this)->id());
  CHECK(timeline_impl);
  scoped_refptr<TimelineTrigger> impl_instance =
      TimelineTrigger::Create(id(), timeline_impl);
  return impl_instance;
}

bool TimelineTrigger::IsTimelineTrigger() const {
  return true;
}

}  // namespace cc
