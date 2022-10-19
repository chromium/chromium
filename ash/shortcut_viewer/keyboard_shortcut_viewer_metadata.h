// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
#define ASH_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_

#include <string>
#include <vector>

#include "ash/shortcut_viewer/ksv_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

struct KeyboardShortcutItem;
enum class ShortcutCategory;

}  // namespace ash

namespace keyboard_shortcut_viewer {

// Returns a list of Ash and Chrome keyboard shortcuts metadata.
KSV_EXPORT const std::vector<ash::KeyboardShortcutItem>&
GetKeyboardShortcutItemList();

std::u16string GetStringForCategory(ash::ShortcutCategory category);

// Certain punctuation is not verbalized by ChromeVox, i.e. ".". So, whenever
// one of these is used in a keyboard shortcut, need to set the accessible name
// to explicitly specified text, such as "period".
// Returns empty string if there is no special accessible name for the
// |key_code|.
std::u16string GetAccessibleNameForKeyboardCode(ui::KeyboardCode key_code);

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
