// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_IME_ARC_IME_UTIL_H_
#define ASH_COMPONENTS_ARC_IME_ARC_IME_UTIL_H_

namespace ui {
class KeyEvent;
}  // namespace ui

namespace arc {

// Returns true if the key event's character is a control character.
// See: https://en.wikipedia.org/wiki/Unicode_control_characters
bool IsControlChar(const ui::KeyEvent* event);

// Returns true if the key event has any modifier.
bool HasModifier(const ui::KeyEvent* event);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_IME_ARC_IME_UTIL_H_
