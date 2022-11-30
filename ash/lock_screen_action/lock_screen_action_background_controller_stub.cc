// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_controller_stub.h"

namespace ash {

LockScreenActionBackgroundControllerStub::
    LockScreenActionBackgroundControllerStub() = default;

LockScreenActionBackgroundControllerStub::
    ~LockScreenActionBackgroundControllerStub() = default;

bool LockScreenActionBackgroundControllerStub::IsBackgroundWindow(
    aura::Window* window) const {
  return false;
}

bool LockScreenActionBackgroundControllerStub::ShowBackground() {
  return false;
}

bool LockScreenActionBackgroundControllerStub::HideBackgroundImmediately() {
  return false;
}

bool LockScreenActionBackgroundControllerStub::HideBackground() {
  return false;
}

}  // namespace ash
