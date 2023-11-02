// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_LOCK_STATE_OBSERVER_H_
#define ASH_WM_LOCK_STATE_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

// Interface for classes that want to be notified by LockStateController when
// session-related events occur.
class ASH_EXPORT LockStateObserver {
 public:
  enum EventType {
    EVENT_PRELOCK_ANIMATION_STARTED,
    EVENT_LOCK_ANIMATION_STARTED,
    EVENT_LOCK_ANIMATION_FINISHED,
  };

  virtual void OnLockStateEvent(EventType event) = 0;
  virtual ~LockStateObserver() {}
};

}  // namespace ash

#endif  // ASH_WM_LOCK_STATE_OBSERVER_H_
