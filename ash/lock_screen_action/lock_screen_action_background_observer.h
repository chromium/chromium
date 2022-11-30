// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_OBSERVER_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_state.h"

namespace ash {

// Used to observe the state of a lock screen action background.
class ASH_EXPORT LockScreenActionBackgroundObserver {
 public:
  virtual ~LockScreenActionBackgroundObserver() {}

  // Called when the state of the lock screen action background changes. For
  // example when the background starts showing (with animation), or a
  // background visibility animation ends.
  virtual void OnLockScreenActionBackgroundStateChanged(
      LockScreenActionBackgroundState state) = 0;
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_OBSERVER_H_
