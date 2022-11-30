// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_STATE_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_STATE_H_

#include "ash/ash_export.h"

namespace ash {

// The state of lock screen action background - the background is shown when a
// lock screen action is activated. It's shown behind the action window and
// its visibility changes can be animated.
//
// The main difference to TrayActionState is what drives the associated state
// machines:
//  * TrayActionStates are controlled by Chrome (and describe the lock screen
//    app state). State changes are one-directional (i.e. they from Chrome to
//    ash).
//  * LockScreenActionBackgroundStates are controlled by ash (and describe the
//    state of the lock screen action background). Note that the background
//    state is ash specific - it's currently not exposed to Chrome in any way.
enum class ASH_EXPORT LockScreenActionBackgroundState {
  // The background is not shown.
  kHidden,

  // The background is visible, and it's "show" animation is in progress (the
  // background can have an animation that's activated as the background is
  // shown). The background will remain in kShowing state until the animation
  // ends.
  // This state is related to TrayActionState::kLaunching in sense that the
  // background is expected to be shown when the note taking app is launched
  // (which is when the note action state changes to kLaunching). Though, the
  // note action can transition to kActive state (which is when the note taking
  // app window is created) both before and after background transitions to
  // kShown state.
  // Note that the background is expected to be in kShowing state only when
  // lock screen note action is either launching or active.
  kShowing,

  // The background is shown, and the animation associate with showing the
  // background has ended.
  // Note that the background is expected to be in kShown state only when
  // lock screen note action is either launching or active.
  kShown,

  // The background is being hidden. The background can have an animation that
  // is activates upon request to hide the background. The background will
  // remain visible, in kHiding state, until the "hide" animation ends.
  // This state is related to lock screen note action states in sense that the
  // background starts hiding when the lock screen note action state changes
  // from kActive or kLaunching state to kAvailable (i.e. inactive state). The
  // background is expected to be in kHiding state only if the lock screen note
  // action is in kAvailable state.
  kHiding
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_STATE_H_
