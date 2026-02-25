// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TRIGGER_H_
#define CC_ANIMATION_ANIMATION_TRIGGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_export.h"
#include "cc/base/protected_sequence_synchronizer.h"

namespace cc {

class AnimationEvents;
class AnimationHost;
class AnimationTriggerDelegate;
struct AnimationTriggerEvent;

// An AnimationTrigger controls the playback of its associated Animations by
// invoking the Animation playback control methods. Each AnimationTrigger
// determines when to invoke these methods based on how the trigger's
// conditions are set up, e.g. a TimelineTrigger acts on its animations
// based on the entry and the exit of portions of its timelines while an
// EventTrigger acts on its associated animations when the UIEvents with
// which the trigger was constructed occur.
class CC_ANIMATION_EXPORT AnimationTrigger
    : public base::RefCounted<AnimationTrigger>,
      public ProtectedSequenceSynchronizer {
 public:
  enum class State {
    // "activate" condition has never been met.
    kIdle,
    // "activate" condition has been met and no "deactivate" condition has been
    // met since the last "activate" condition was met.
    kPrimary,
    // "deactivate" condition has been met and no "activate" condition has been
    // met since the last "deactivate" condition was met.
    kInverse,
  };

  enum class Behavior {
    kPlay,
    kPause,
    kReset,
    kPlayOnce,
    kPlayBackwards,
    kPlayForwards,
    kReplay,
    kNone,
  };

  struct CC_ANIMATION_EXPORT AnimationData {
    AnimationData(int animation_id,
                  int timeline_id,
                  Behavior activate_behavior,
                  Behavior deactivate_behavior);
    AnimationData(const AnimationData& data);
    ~AnimationData();

    // The id of the animation to be played, paused, etc.
    int animation_id;

    // The id of the timeline of the animation to be played, etc.
    int timeline_id;

    Behavior activate_behavior;
    Behavior deactivate_behavior;

    bool operator==(const AnimationData&) const;
  };

  AnimationTrigger(const AnimationTrigger&) = delete;
  AnimationTrigger& operator=(const AnimationTrigger&) = delete;

  virtual scoped_refptr<AnimationTrigger> CreateImplInstance(
      AnimationHost& host) const = 0;

  virtual bool IsEventTrigger() const;
  virtual bool IsTimelineTrigger() const;

  int id() const { return id_; }

  AnimationHost* GetAnimationHost() { return animation_host_; }
  void SetAnimationHost(AnimationHost* animation_host) {
    animation_host_ = animation_host;
  }

  // ProtectedSequenceSynchronizer implementation
  bool IsOwnerThread() const override;
  bool InProtectedSequence() const override;
  void WaitForProtectedSequenceCompletion() const override;

  void SetAnimationData(std::vector<AnimationData>& data);
  const std::vector<AnimationData>& GetAnimationDataForTest();

  void PushPropertiesTo(AnimationTrigger* trigger_impl);
  void SetNeedsPushProperties();

  // Perform the relevant actions for the associated animations based on the
  // trigger's state.
  void PerformActivate(AnimationEvents* events);
  void PerformDeactivate(AnimationEvents* events);

  void SetAnimationTriggerDelegate(AnimationTriggerDelegate* delegate);

  // Dispatches animation trigger events (activate/deactivate) to the
  // animation trigger delegate (the main thread trigger).
  void DispatchAnimationTriggerEvent(const AnimationTriggerEvent& event);

 protected:
  explicit AnimationTrigger(int id);
  ~AnimationTrigger() override;

 private:
  friend class base::RefCounted<AnimationTrigger>;

  const int id_;

  raw_ptr<AnimationHost> animation_host_;
  ProtectedSequenceReadable<std::vector<AnimationData>> animation_data_;
  // The main thread trigger listening for compositor-initiated trigger events.
  raw_ptr<AnimationTriggerDelegate> animation_trigger_delegate_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TRIGGER_H_
