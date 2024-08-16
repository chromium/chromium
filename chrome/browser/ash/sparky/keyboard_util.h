// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SPARKY_KEYBOARD_UTIL_H_
#define CHROME_BROWSER_ASH_SPARKY_KEYBOARD_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "ui/events/event.h"

namespace ash {

// Returns a pressed/released pair of KeyEvents for `key_code`, with some
// combination of control, alt, shift held down.
std::pair<ui::KeyEvent, ui::KeyEvent>
MakeKeyEventPair(ui::KeyboardCode key_code, bool control, bool alt, bool shift);

// Given some `text`, returns a vector of key events which simulate typing that
// text on a US keyboard. The returned vector contains a pressed/released
// pair of events for each character in `text`, and handles the modifiers
// for uppercase characters.
//
// This only works for characters typeable on a US keyboard. If any other
// character is encountered, it will return nullopt.
std::optional<std::vector<ui::KeyEvent>> KeyEventsForText(
    const std::string& text);

// Returns the KeyEvent corresponding to a subset of the key strings defined
// in https://www.w3.org/TR/uievents-key/, such as "Tab" and "Enter". Returns
// nullopt if `key` is invalid or unimplemented.
//
// This function is case insensitive.
std::optional<ui::KeyboardCode> KeyboardCodeForDOMString(
    const std::string& key);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SPARKY_KEYBOARD_UTIL_H_
