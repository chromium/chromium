// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_state_type.h"

#include "ash/public/interfaces/window_state_type.mojom.h"

namespace ash {

mojom::WindowStateType ToWindowStateType(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return mojom::WindowStateType::DEFAULT;
    case ui::SHOW_STATE_NORMAL:
      return mojom::WindowStateType::NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return mojom::WindowStateType::MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return mojom::WindowStateType::MAXIMIZED;
    case ui::SHOW_STATE_INACTIVE:
      return mojom::WindowStateType::INACTIVE;
    case ui::SHOW_STATE_FULLSCREEN:
      return mojom::WindowStateType::FULLSCREEN;
    case ui::SHOW_STATE_END:
      NOTREACHED();
      return mojom::WindowStateType::DEFAULT;
  }
}

ui::WindowShowState ToWindowShowState(mojom::WindowStateType type) {
  switch (type) {
    case mojom::WindowStateType::DEFAULT:
      return ui::SHOW_STATE_DEFAULT;
    case mojom::WindowStateType::NORMAL:
    case mojom::WindowStateType::RIGHT_SNAPPED:
    case mojom::WindowStateType::LEFT_SNAPPED:
    case mojom::WindowStateType::AUTO_POSITIONED:
    case mojom::WindowStateType::PIP:
      return ui::SHOW_STATE_NORMAL;

    case mojom::WindowStateType::MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case mojom::WindowStateType::MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case mojom::WindowStateType::INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case mojom::WindowStateType::FULLSCREEN:
    case mojom::WindowStateType::PINNED:
    case mojom::WindowStateType::TRUSTED_PINNED:
      return ui::SHOW_STATE_FULLSCREEN;
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsFullscreenOrPinnedWindowStateType(mojom::WindowStateType type) {
  return type == mojom::WindowStateType::FULLSCREEN ||
         type == mojom::WindowStateType::PINNED ||
         type == mojom::WindowStateType::TRUSTED_PINNED;
}

bool IsMaximizedOrFullscreenOrPinnedWindowStateType(
    mojom::WindowStateType type) {
  return type == mojom::WindowStateType::MAXIMIZED ||
         IsFullscreenOrPinnedWindowStateType(type);
}

bool IsMinimizedWindowStateType(mojom::WindowStateType type) {
  return type == mojom::WindowStateType::MINIMIZED;
}

bool IsValidWindowStateType(int64_t value) {
  return value == int64_t(mojom::WindowStateType::DEFAULT) ||
         value == int64_t(mojom::WindowStateType::NORMAL) ||
         value == int64_t(mojom::WindowStateType::MINIMIZED) ||
         value == int64_t(mojom::WindowStateType::MAXIMIZED) ||
         value == int64_t(mojom::WindowStateType::INACTIVE) ||
         value == int64_t(mojom::WindowStateType::FULLSCREEN) ||
         value == int64_t(mojom::WindowStateType::LEFT_SNAPPED) ||
         value == int64_t(mojom::WindowStateType::RIGHT_SNAPPED) ||
         value == int64_t(mojom::WindowStateType::AUTO_POSITIONED) ||
         value == int64_t(mojom::WindowStateType::PINNED) ||
         value == int64_t(mojom::WindowStateType::TRUSTED_PINNED) ||
         value == int64_t(mojom::WindowStateType::PIP);
}

}  // namespace ash
