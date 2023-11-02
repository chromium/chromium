// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
#define ASH_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_

#include <string>
#include <vector>

#include "ash/shortcut_viewer/ksv_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

struct KeyboardShortcutItem;
enum class ShortcutCategory;

}  // namespace ash

namespace keyboard_shortcut_viewer {

// Returns a list of Ash and Chrome keyboard shortcuts metadata.
KSV_EXPORT const std::vector<ash::KeyboardShortcutItem>&
GetKeyboardShortcutItemList();

std::u16string GetStringForCategory(ash::ShortcutCategory category);

// Returns the string of a DomKey for a given VKEY. VKEY needs to be mapped to
// a physical key |dom_code| and then the |dom_code| needs to be mapped to a
// meaning or character of |dom_key| based on the corresponding keyboard layout.
//
// For shortcuts based on keys that use positional mapping, eg. plus, minus,
// left/right bracket, comma, period, or slash the VKEY is mapped to the
// glyph on the key with the same physical position as the US layout. This
// remapping can be disabled by passing false to |remap_positional_key|. This
// is currently used for the 2 browser shortcuts that use these keys but are
// not remapped (page zoom in/out).
//
// Returns empty string if the |dom_key| has no mapped meaning or character.
std::u16string GetStringForKeyboardCode(ui::KeyboardCode key_code,
                                        bool remap_positional_key = true);

// Certain punctuation is not verbalized by ChromeVox, i.e. ".". So, whenever
// one of these is used in a keyboard shortcut, need to set the accessible name
// to explicitly specified text, such as "period".
// Returns empty string if there is no special accessible name for the
// |key_code|.
std::u16string GetAccessibleNameForKeyboardCode(ui::KeyboardCode key_code);

// Returns the VectorIcon if |key_code| need to be represented as an icon.
// Returns nullptr if |key_code| should not be represented as an icon.
const gfx::VectorIcon* GetVectorIconForKeyboardCode(ui::KeyboardCode key_code);

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
