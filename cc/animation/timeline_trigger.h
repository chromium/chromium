// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TIMELINE_TRIGGER_H_
#define CC_ANIMATION_TIMELINE_TRIGGER_H_

#include "cc/animation/animation_trigger.h"

namespace cc {

class AnimationTimeline;
class ScrollTree;

class CC_ANIMATION_EXPORT TimelineTrigger : public AnimationTrigger {
 public:
  using State = AnimationTrigger::State;

  // Structure to hold the boundaries of the trigger, in units of time -
  // the units of the timeline's CurrentTime.
  struct Boundaries {
    // These denote the start and end of the activation range
    base::TimeTicks activation_start_time;
    base::TimeTicks activation_end_time;
    // These denote the start and end of the active range.
    base::TimeTicks active_start_time;
    base::TimeTicks active_end_time;
  };

  static scoped_refptr<TimelineTrigger> Create(
      int id,
      scoped_refptr<AnimationTimeline> timeline,
      Boundaries Boundaries);

  scoped_refptr<AnimationTrigger> CreateImplInstance(
      AnimationHost& host_impl) const override;

  bool IsTimelineTrigger() const override;

  // Update the state of the trigger based on the current time of the timeline,
  // queueing up trigger events and triggering animations if necessary.
  void Update(const ScrollTree& scroll_tree, AnimationEvents* events);

 protected:
  explicit TimelineTrigger(int id,
                           scoped_refptr<AnimationTimeline> timeline,
                           Boundaries boundaries);
  ~TimelineTrigger() override;

 private:
  friend class RefCounted<TimelineTrigger>;

  State ComputeState(base::TimeTicks time);
  bool InActivationRange(base::TimeTicks time);
  bool InActiveRange(base::TimeTicks time);

  ProtectedSequenceReadable<scoped_refptr<AnimationTimeline>> timeline_;

  // The boundaries of the associated timeline that define the enter and exit
  // ranges.
  Boundaries boundaries_;
  // The most recently observed state of the trigger.
  State state_ = State::kIdle;
};

}  // namespace cc

#endif  // CC_ANIMATION_TIMELINE_TRIGGER_H_
