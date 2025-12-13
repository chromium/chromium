// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TRIGGER_H_
#define CC_ANIMATION_ANIMATION_TRIGGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_export.h"
#include "cc/base/protected_sequence_synchronizer.h"

namespace cc {

class AnimationHost;

// An AnimationTrigger controls the playback of its associated Animations by
// invoking the Animation playback control methods. Each AnimationTrigger
// determines when to invoke these methods based on how the trigger's conditions
// are set up, e.g. a TimelineTrigger acts on its animations based on the entry
// and the exit of portions of its timelines while an EventTrigger acts on its
// associated animations when the UIEvents with which the trigger was
// constructed occur.
class CC_ANIMATION_EXPORT AnimationTrigger
    : public base::RefCounted<AnimationTrigger>,
      public ProtectedSequenceSynchronizer {
 public:
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

  void SetNeedsPushProperties();

 protected:
  explicit AnimationTrigger(int id);
  ~AnimationTrigger() override;

 private:
  friend class base::RefCounted<AnimationTrigger>;

  const int id_;

  raw_ptr<AnimationHost> animation_host_;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TRIGGER_H_
