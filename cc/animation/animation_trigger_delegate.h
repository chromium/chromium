// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TRIGGER_DELEGATE_H_
#define CC_ANIMATION_ANIMATION_TRIGGER_DELEGATE_H_

namespace cc {

// This class describes an interface to be implemented by objects
// interested in listening to "activate" and "deactivate" events of
// cc AnimationTriggers.
class CC_ANIMATION_EXPORT AnimationTriggerDelegate {
 public:
  virtual void NotifyActivated() = 0;
  virtual void NotifyDeactivated() = 0;

 protected:
  ~AnimationTriggerDelegate() = default;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TRIGGER_DELEGATE_H_
