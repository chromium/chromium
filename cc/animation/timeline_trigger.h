// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_TIMELINE_TRIGGER_H_
#define CC_ANIMATION_TIMELINE_TRIGGER_H_

#include "cc/animation/animation_trigger.h"

namespace cc {

class AnimationTimeline;

class CC_ANIMATION_EXPORT TimelineTrigger : public AnimationTrigger {
 public:
  static scoped_refptr<TimelineTrigger> Create(
      int id,
      scoped_refptr<AnimationTimeline> timeline);

  scoped_refptr<AnimationTrigger> CreateImplInstance(
      AnimationHost& host_impl) const override;

  bool IsTimelineTrigger() const override;

 protected:
  explicit TimelineTrigger(int id, scoped_refptr<AnimationTimeline> timeline);
  ~TimelineTrigger() override;

 private:
  friend class RefCounted<TimelineTrigger>;

  ProtectedSequenceReadable<scoped_refptr<AnimationTimeline>> timeline_;
};

}  // namespace cc

#endif  // CC_ANIMATION_TIMELINE_TRIGGER_H_
