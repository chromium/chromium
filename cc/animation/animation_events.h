// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_EVENTS_H_
#define CC_ANIMATION_ANIMATION_EVENTS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/trees/mutator_host.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"

namespace cc {

struct CC_ANIMATION_EXPORT AnimationEvent {
  enum class Type { kStarted, kFinished, kAborted, kTakeOver, kTimeUpdated };

  typedef size_t KeyframeEffectId;
  struct UniqueKeyframeModelId {
    int timeline_id;
    int animation_id;
    int model_id;
  };

  AnimationEvent(Type type,
                 UniqueKeyframeModelId uid,
                 int group_id,
                 int target_property,
                 base::TimeTicks monotonic_time);

  // Constructs AnimationEvent of TIME_UPDATED type.
  AnimationEvent(int timeline_id,
                 int animation_id,
                 std::optional<base::TimeDelta> local_time);

  AnimationEvent(const AnimationEvent& other);
  AnimationEvent& operator=(const AnimationEvent& other);

  ~AnimationEvent();

  bool ShouldDispatchToKeyframeEffectAndModel() const;

  Type type;
  UniqueKeyframeModelId uid;
  int group_id;
  int target_property;
  base::TimeTicks monotonic_time;
  bool is_impl_only;

  // For continuing a scroll offset animation on the main thread.
  base::TimeTicks animation_start_time;
  std::unique_ptr<gfx::AnimationCurve> curve;

  // Set for TIME_UPDATED events.
  std::optional<base::TimeDelta> local_time;
};

class CC_ANIMATION_EXPORT AnimationEvents : public MutatorEvents {
 public:
  AnimationEvents();

  // MutatorEvents implementation.
  ~AnimationEvents() override;
  bool IsEmpty() const override;

  bool needs_time_updated_events() const { return needs_time_updated_events_; }
  void set_needs_time_updated_events(bool value) {
    needs_time_updated_events_ = value;
  }

  const std::vector<AnimationEvent>& events() const { return events_; }
  std::vector<AnimationEvent>& events() { return events_; }

 private:
  std::vector<AnimationEvent> events_;
  bool needs_time_updated_events_ = false;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_EVENTS_H_
