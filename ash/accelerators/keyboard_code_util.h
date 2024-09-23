// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_KEYBOARD_CODE_UTIL_H_
#define ASH_ACCELERATORS_KEYBOARD_CODE_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// Returns the string of a DomKey for a given VKEY. VKEY needs to be mapped to
// a physical key `dom_code` and then the `dom_code` needs to be mapped to a
// meaning or character of `dom_key` based on the corresponding keyboard layout.
//
// For shortcuts based on keys that use positional mapping, eg. plus, minus,
// left/right bracket, comma, period, or slash the VKEY is mapped to the
// glyph on the key with the same physical position as the US layout. This
// remapping can be disabled by passing false to `remap_positional_key`. This
// is currently used for the 2 browser shortcuts that use these keys but are
// not remapped (page zoom in/out).
//
// Returns empty string if the `dom_key` has no mapped meaning or character.
ASH_EXPORT std::u16string GetStringForKeyboardCode(
    ui::KeyboardCode key_code,
    bool remap_positional_key = true);

// Returns the VectorIcon if `key_code` needs to be represented as an icon.
// Returns nullptr if `key_code` should not be represented as an icon.
ASH_EXPORT const gfx::VectorIcon* GetVectorIconForKeyboardCode(
    ui::KeyboardCode key_code);

// Returns the corresponding vector icon for search or launcher key depending on
// the keyboard layout and whether the assistant is enabled or not.
ASH_EXPORT const gfx::VectorIcon* GetSearchOrLauncherVectorIcon();

}  // namespace ash

#endif  // ASH_ACCELERATORS_KEYBOARD_CODE_UTIL_H_
