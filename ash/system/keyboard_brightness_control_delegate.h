// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_

namespace ash {

// Delegate for controlling the keyboard brightness.
class KeyboardBrightnessControlDelegate {
 public:
  virtual ~KeyboardBrightnessControlDelegate() {}

  // Handles an accelerator-driven request to decrease or increase
  // the keyboard brightness, or toggle the backlight itself on/off.
  virtual void HandleKeyboardBrightnessDown() = 0;
  virtual void HandleKeyboardBrightnessUp() = 0;
  virtual void HandleToggleKeyboardBacklight() = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_
