// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONTROLLER_OBSERVER_H_

#include <set>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Describes the various attributes of the keyboard's appearance and usability.
struct KeyboardStateDescriptor {
  bool is_visible;

  // The bounds of the keyboard window on the screen.
  gfx::Rect visual_bounds;

  // The bounds of the area on the screen that is considered "blocked" by the
  // keyboard. For example, the docked keyboard's occluded bounds is the same as
  // the visual bounds, but the floating keyboard has no occluded bounds (as the
  // window is small and moveable).
  gfx::Rect occluded_bounds_in_screen;

  // The bounds of the area on the screen that is considered "unusable" because
  // it is blocked by the keyboard. This is used by the accessibility keyboard.
  gfx::Rect displaced_bounds_in_screen;
};

class ASH_PUBLIC_EXPORT KeyboardControllerObserver {
 public:
  // Called when a keyboard enable flag changes.
  virtual void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) {}

  // Called when the keyboard is enabled or disabled. If ReloadKeyboard() is
  // called or other code enables the keyboard while already enabled, this will
  // be called twice, once when the keyboard is disabled and again when it is
  // re-enabled.
  virtual void OnKeyboardEnabledChanged(bool is_enabled) {}

  // Called when the virtual keyboard configuration changes.
  virtual void OnKeyboardConfigChanged(const keyboard::KeyboardConfig& config) {
  }

  // Called when the visibility of the virtual keyboard changes, e.g. an input
  // field is focused or blurred, or the user hides the keyboard.
  virtual void OnKeyboardVisibilityChanged(bool visible) {}

  // Called when the keyboard bounds change. |screen_bounds| is in screen
  // coordinates.
  virtual void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) {}

  // Called when the keyboard bounds have changed in a way that should affect
  // the usable region of the workspace. The user interface should respond to
  // this event by moving important elements away from |new_bounds_in_screen|
  // so that they don't overlap. However, drastic visual changes should be
  // avoided, as the occluded bounds may change frequently.
  virtual void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) {
  }

  // Called when the keyboard bounds have changed in a way that affects how the
  // workspace should change to not take up the screen space occupied by the
  // keyboard. The user interface should respond to this event by moving all
  // elements away from |new_bounds| so that they don't overlap. Large visual
  // changes are okay, as the displacing bounds do not change frequently.
  virtual void OnKeyboardDisplacingBoundsChanged(const gfx::Rect& new_bounds) {}

  // Redundant with other various notification methods. Use this if the state of
  // multiple properties need to be conveyed simultaneously to observer
  // implementations without the need to track multiple stateful properties.
  virtual void OnKeyboardAppearanceChanged(
      const KeyboardStateDescriptor& state) {}

  // Signals a request to load the keyboard contents. If the contents are
  // already loaded, requests a reload. Once the contents have loaded,
  // KeyboardController.KeyboardContentsLoaded is expected to be called by the
  // client implementation.
  virtual void OnLoadKeyboardContentsRequested() {}

  // Called when the UI has been destroyed so that the client can reset the
  // embedded contents and handle.
  virtual void OnKeyboardUIDestroyed() {}

  // Called when the keyboard has been hidden and the hiding animation finished
  // successfully.
  // When |is_temporary_hide| is true, this hide is immediately followed by a
  // show (e.g. when changing to floating keyboard)
  virtual void OnKeyboardHidden(bool is_temporary_hide) {}

 protected:
  virtual ~KeyboardControllerObserver() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_KEYBOARD_KEYBOARD_CONTROLLER_OBSERVER_H_
