// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_UTIL_H_
#define ASH_KEYBOARD_UI_KEYBOARD_UTIL_H_

#include <string>

#include "ash/keyboard/ui/keyboard_export.h"
#include "base/strings/string16.h"

// Global utility functions for the virtual keyboard.
// TODO(stevenjb/shuchen/shend): Many of these are accessed from both Chrome
// and Ash. We need to remove any Chrome dependencies. htpps://crbug.com/843332

namespace keyboard {

// Sets the state of the a11y onscreen keyboard.
KEYBOARD_EXPORT void SetAccessibilityKeyboardEnabled(bool enabled);

// Gets the state of the a11y onscreen keyboard.
KEYBOARD_EXPORT bool GetAccessibilityKeyboardEnabled();

// Sets whether the keyboard is enabled from the shelf.
KEYBOARD_EXPORT void SetKeyboardEnabledFromShelf(bool enabled);

// Gets whether the keyboard is enabled from the shelf.
KEYBOARD_EXPORT bool GetKeyboardEnabledFromShelf();

// Sets the state of the touch onscreen keyboard.
KEYBOARD_EXPORT void SetTouchKeyboardEnabled(bool enabled);

// Gets the state of the touch onscreen keyboard.
KEYBOARD_EXPORT bool GetTouchKeyboardEnabled();

// Returns true if the virtual keyboard is enabled.
KEYBOARD_EXPORT bool IsKeyboardEnabled();

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_UTIL_H_
