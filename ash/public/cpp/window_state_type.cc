// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/window_state_type.h"

#include "base/logging.h"

namespace ash {

std::ostream& operator<<(std::ostream& stream, WindowStateType state) {
  switch (state) {
    case WindowStateType::kDefault:
      return stream << "kDefault";
    case WindowStateType::kNormal:
      return stream << "kNormal";
    case WindowStateType::kMinimized:
      return stream << "kMinimized";
    case WindowStateType::kMaximized:
      return stream << "kMaximized";
    case WindowStateType::kInactive:
      return stream << "kInactive";
    case WindowStateType::kFullscreen:
      return stream << "kFullscreen";
    case WindowStateType::kLeftSnapped:
      return stream << "kLeftSnapped";
    case WindowStateType::kRightSnapped:
      return stream << "kRightSnapped";
    case WindowStateType::kAutoPositioned:
      return stream << "kAutoPositioned";
    case WindowStateType::kPinned:
      return stream << "kPinned";
    case WindowStateType::kTrustedPinned:
      return stream << "kTrustedPinned";
    case WindowStateType::kPip:
      return stream << "kPip";
  }

  NOTREACHED();
  return stream;
}

WindowStateType ToWindowStateType(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return WindowStateType::kDefault;
    case ui::SHOW_STATE_NORMAL:
      return WindowStateType::kNormal;
    case ui::SHOW_STATE_MINIMIZED:
      return WindowStateType::kMinimized;
    case ui::SHOW_STATE_MAXIMIZED:
      return WindowStateType::kMaximized;
    case ui::SHOW_STATE_INACTIVE:
      return WindowStateType::kInactive;
    case ui::SHOW_STATE_FULLSCREEN:
      return WindowStateType::kFullscreen;
    case ui::SHOW_STATE_END:
      NOTREACHED();
      return WindowStateType::kDefault;
  }
}

ui::WindowShowState ToWindowShowState(WindowStateType type) {
  switch (type) {
    case WindowStateType::kDefault:
      return ui::SHOW_STATE_DEFAULT;
    case WindowStateType::kNormal:
    case WindowStateType::kRightSnapped:
    case WindowStateType::kLeftSnapped:
    case WindowStateType::kAutoPositioned:
    case WindowStateType::kPip:
      return ui::SHOW_STATE_NORMAL;

    case WindowStateType::kMinimized:
      return ui::SHOW_STATE_MINIMIZED;
    case WindowStateType::kMaximized:
      return ui::SHOW_STATE_MAXIMIZED;
    case WindowStateType::kInactive:
      return ui::SHOW_STATE_INACTIVE;
    case WindowStateType::kFullscreen:
    case WindowStateType::kPinned:
    case WindowStateType::kTrustedPinned:
      return ui::SHOW_STATE_FULLSCREEN;
  }
  NOTREACHED();
  return ui::SHOW_STATE_DEFAULT;
}

bool IsFullscreenOrPinnedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kFullscreen ||
         type == WindowStateType::kPinned ||
         type == WindowStateType::kTrustedPinned;
}

bool IsMaximizedOrFullscreenOrPinnedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kMaximized ||
         IsFullscreenOrPinnedWindowStateType(type);
}

bool IsMinimizedWindowStateType(WindowStateType type) {
  return type == WindowStateType::kMinimized;
}

bool IsValidWindowStateType(int64_t value) {
  return value == int64_t(WindowStateType::kDefault) ||
         value == int64_t(WindowStateType::kNormal) ||
         value == int64_t(WindowStateType::kMinimized) ||
         value == int64_t(WindowStateType::kMaximized) ||
         value == int64_t(WindowStateType::kInactive) ||
         value == int64_t(WindowStateType::kFullscreen) ||
         value == int64_t(WindowStateType::kLeftSnapped) ||
         value == int64_t(WindowStateType::kRightSnapped) ||
         value == int64_t(WindowStateType::kAutoPositioned) ||
         value == int64_t(WindowStateType::kPinned) ||
         value == int64_t(WindowStateType::kTrustedPinned) ||
         value == int64_t(WindowStateType::kPip);
}

}  // namespace ash
