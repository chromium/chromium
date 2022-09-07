// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_LAYOUT_DELEGATE_H_
#define ASH_KEYBOARD_UI_KEYBOARD_LAYOUT_DELEGATE_H_

#include <stdint.h>

#include "ash/keyboard/ui/keyboard_export.h"
#include "ui/events/event.h"

namespace display {
class Display;
}

namespace aura {
class Window;
}

namespace keyboard {

// A delegate class to control the virtual keyboard layout
class KEYBOARD_EXPORT KeyboardLayoutDelegate {
 public:
  virtual ~KeyboardLayoutDelegate() {}

  // Get the container window where the virtual keyboard show appear by default.
  // Usually, this would be a touchable display with input focus.
  // This function must not return null.
  virtual aura::Window* GetContainerForDefaultDisplay() = 0;

  // Get the container window for a particular display. |display| must be valid.
  virtual aura::Window* GetContainerForDisplay(
      const display::Display& display) = 0;

  // Transfer a gesture event to the Ash shelf. Any remaining gestures will be
  // sent directly to the shelf. Used for accessing the shelf and the home
  // screen even when the virtual keyboard is blocking the shelf.
  virtual void TransferGestureEventToShelf(const ui::GestureEvent& e) = 0;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_LAYOUT_DELEGATE_H_
