// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_pin_util.h"

#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "ui/aura/window.h"

void PinWindow(aura::Window* window, bool trusted) {
  DCHECK(window);
  ash::window_util::PinWindow(window, trusted);
}

void UnpinWindow(aura::Window* window) {
  DCHECK(window);
  ash::WindowState::Get(window)->Restore();
}

chromeos::WindowPinType GetWindowPinType(const aura::Window* window) {
  DCHECK(window);

  const ash::WindowState* window_state = ash::WindowState::Get(window);
  if (!window_state) {
    return chromeos::WindowPinType::kNone;
  }

  if (window_state->IsTrustedPinned()) {
    return chromeos::WindowPinType::kTrustedPinned;
  }

  if (window_state->IsPinned()) {
    return chromeos::WindowPinType::kPinned;
  }

  return chromeos::WindowPinType::kNone;
}

bool IsWindowPinned(const aura::Window* window) {
  DCHECK(window);
  const ash::WindowState* state = ash::WindowState::Get(window);
  return state->IsPinned();
}
