// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_EVENTS_H_
#define CC_ANIMATION_ANIMATION_EVENTS_H_

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/trees/mutator_host.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"

namespace cc {

struct CC_ANIMATION_EXPORT AnimationPlaybackEvent {
  enum class Type { kStarted, kFinished, kAborted, kTakeOver, kTimeUpdated };

  typedef size_t KeyframeEffectId;
  struct UniqueKeyframeModelId {
    int timeline_id;
    int animation_id;
    int model_id;
  };

  AnimationPlaybackEvent(Type type,
                         UniqueKeyframeModelId uid,
                         int group_id,
                         int target_property,
                         base::TimeTicks monotonic_time);

  // Constructs AnimationPlaybackEvent of TIME_UPDATED type.
  AnimationPlaybackEvent(int timeline_id,
                         int animation_id,
                         std::optional<base::TimeDelta> local_time);

  AnimationPlaybackEvent(const AnimationPlaybackEvent& other);
  AnimationPlaybackEvent& operator=(const AnimationPlaybackEvent& other);

  ~AnimationPlaybackEvent();

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

  std::optional<base::TimeDelta> local_time;
};

// This describes the occurrence of an event for an animation-trigger[1]
// that occurs on the impl thread.
// [1] https://drafts.csswg.org/css-animations-2/#animation-triggers
struct CC_ANIMATION_EXPORT AnimationTriggerEvent {
  enum class Type {
    kActivate,
    kDeactivate,
  };

  AnimationTriggerEvent(int trigger_id, Type type);
  AnimationTriggerEvent(const AnimationTriggerEvent& other);

  int trigger_id;
  Type type;
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

  using Event = std::variant<AnimationPlaybackEvent, AnimationTriggerEvent>;

  const std::vector<Event>& events() const { return events_; }
  std::vector<Event>& events() { return events_; }

 private:
  std::vector<Event> events_;
  bool needs_time_updated_events_ = false;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_EVENTS_H_
