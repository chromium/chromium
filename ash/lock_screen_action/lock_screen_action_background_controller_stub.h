// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_STUB_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_STUB_H_

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller.h"

namespace ash {

// Stub lock screen action background controller - used when no background
// is shown behind the lock screen action app window, as is the case when
// --show-md-login flag is not set. The controller will forever remain in hidden
// state.
class ASH_EXPORT LockScreenActionBackgroundControllerStub
    : public LockScreenActionBackgroundController {
 public:
  LockScreenActionBackgroundControllerStub();

  LockScreenActionBackgroundControllerStub(
      const LockScreenActionBackgroundControllerStub&) = delete;
  LockScreenActionBackgroundControllerStub& operator=(
      const LockScreenActionBackgroundControllerStub&) = delete;

  ~LockScreenActionBackgroundControllerStub() override;

  // LockScreenActionBackgroundController:
  bool IsBackgroundWindow(aura::Window* window) const override;
  bool ShowBackground() override;
  bool HideBackgroundImmediately() override;
  bool HideBackground() override;
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_CONTROLLER_STUB_H_
