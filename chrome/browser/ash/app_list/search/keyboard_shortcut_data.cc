// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_data.h"

#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/string_util.h"

namespace app_list {

KeyboardShortcutData::KeyboardShortcutData(
    const ash::KeyboardShortcutItem& item)
    : description_message_id(item.description_message_id),
      description(base::CollapseWhitespace(
          l10n_util::GetStringUTF16(item.description_message_id),
          true)),
      shortcut_message_id(item.shortcut_message_id),
      shortcut_key_codes(item.shortcut_key_codes) {
  if (description_message_id == IDS_KSV_DESCRIPTION_TAKE_SCREENSHOT) {
    // This shortcut has a special-case modification to display an alternative
    // shortcut. This is because its default keycodes contains a key that might
    // not be present on all keyboards.
    // Also note that VKEY_UNKNOWN is a stand-in for the + operator.
    shortcut_message_id = IDS_APP_LIST_SEARCH_SCREENSHOT_SHORTCUT;
    shortcut_key_codes = {ui::VKEY_SNAPSHOT, ui::VKEY_CONTROL,
                          ui::VKEY_UNKNOWN,  ui::VKEY_SHIFT,
                          ui::VKEY_UNKNOWN,  ui::VKEY_MEDIA_LAUNCH_APP1};
  }
}

KeyboardShortcutData::KeyboardShortcutData(const KeyboardShortcutData&) =
    default;

KeyboardShortcutData::~KeyboardShortcutData() = default;

}  // namespace app_list
