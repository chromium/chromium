// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/keyboard_shortcut_item.h"

#include <tuple>

#include "base/check.h"

namespace keyboard_shortcut_viewer {

bool AcceleratorId::operator<(const AcceleratorId& other) const {
  return std::tie(keycode, modifiers) <
         std::tie(other.keycode, other.modifiers);
}

KeyboardShortcutItem::KeyboardShortcutItem(
    const std::vector<ShortcutCategory>& categories,
    int description_message_id,
    base::Optional<int> shortcut_message_id,
    const std::vector<AcceleratorId>& accelerator_ids,
    const std::vector<ui::KeyboardCode>& shortcut_key_codes)
    : categories(categories),
      description_message_id(description_message_id),
      shortcut_message_id(shortcut_message_id),
      accelerator_ids(accelerator_ids),
      shortcut_key_codes(shortcut_key_codes) {
  DCHECK(!categories.empty());
}

KeyboardShortcutItem::KeyboardShortcutItem(const KeyboardShortcutItem& other) =
    default;

KeyboardShortcutItem::~KeyboardShortcutItem() = default;

}  // namespace keyboard_shortcut_viewer
