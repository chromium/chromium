// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_ITEM_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_ITEM_H_

#include <vector>

#include "ash/components/shortcut_viewer/ksv_export.h"
#include "base/macros.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace keyboard_shortcut_viewer {

// The categories the shortcut belongs to. Used to group shortcuts in the
// viewer. This order is significant as it determines the order in which the
// categories will be displayed in the view.
enum class ShortcutCategory {
  kUnknown = 0,
  kPopular,
  kTabAndWindow,
  kPageAndBrowser,
  kSystemAndDisplay,
  kTextEditing,
  kAccessibility,
};

// AcceleratorId describes an accelerator key + modifiers combination. //ash
// and //chrome provide helpers for working with accelerator combinations but
// we cannot depend on them here. The key + modifiers are also used to populate
// the values of |shortcut_key_codes| field in KeyboardShortcutItem, in order to
// show the key and modifiers in the viewer with different styles.
struct KSV_EXPORT AcceleratorId {
  bool operator<(const AcceleratorId& other) const;

  ui::KeyboardCode keycode = ui::VKEY_UNKNOWN;
  int modifiers = 0;
};

// Metadata about a VKEY-based keyboard shortcut in ui::Accelerator tables in
// both //ash and //chrome, including descriptive string resources ids.
// For example, a shortcut with description string of "Go to previous
// page in your browsing history" has shortcut string of "Alt + left arrow" and
// the category of Category::kTabAndWindow.
// To make the replacement parts highlighted in the UI, in this case, the I18n
// string of the shortcut is "<ph name="modifier">$1<ex>ALT</ex></ph> +
// <ph name="key">$2<ex>V</ex></ph>". The l10n_util::GetStringFUTF16() will
// return the offsets of the replacements, which are used to generate style
// ranges to insert symbols for the modifiers and key. The first placeholder
// (modifier) will be replaced by text "Alt" for search. The second placeholder
// (key) will be replaced by text "left arrow" so that users can search by
// "left" and/or "arrow". But the UI representation of the key is an icon of
// "left arrow".
struct KSV_EXPORT KeyboardShortcutItem {
  KeyboardShortcutItem(
      const std::vector<ShortcutCategory>& categories,
      int description_message_id,
      int shortcut_message_id,
      const std::vector<AcceleratorId>& accelerator_ids = {},
      const std::vector<ui::KeyboardCode>& shortcut_key_codes = {});
  explicit KeyboardShortcutItem(const KeyboardShortcutItem& other);
  ~KeyboardShortcutItem();

  // The categories this shortcut belongs to.
  std::vector<ShortcutCategory> categories;

  // Id of the message resource describing what action the shortcut performs.
  int description_message_id;

  // Id of the message template resource used to list the keys making up the
  // shortcut.
  int shortcut_message_id;

  // Multiple accelerators can be mapped to the same KeyboardShortcutItem.
  // |shortcut_key_codes| could be auto-generated from |accelerator_ids| to
  // avoid duplicates.
  //  There are four rules:
  //  1. For regular accelerators, we show all modifiers and key in order, then
  //     we can auto-generate it by |accelerator_ids| (actually only one
  //     accelerator_id in this case).
  //  2. For grouped |accelerator_ids| with the same modifiers, we can
  //     auto-generate the modifiers. We will not show the key.
  //     E.g. shortcuts for "CTRL + 1 through 8.", we will provide
  //     shortcut_string " + 1 through 8", and auto-generate {ui::VKEY_CONTROL}.
  //  3. For grouped |accelerator_ids| with different modifiers, e.g.
  //     TOGGLE_FULLSCREEN, we can not auto-generate it and we will provide
  //     the |shortcut_key_codes|.
  //  4. For ksv items not in the two accelerator_tables, we will provide the
  //     |shortcut_key_codes| and |accelerator_ids| will be empty.
  std::vector<AcceleratorId> accelerator_ids;

  // The VKEY codes of the key and each modifier comprising the shortcut. These
  // are translated to text or icons representing each key, and substituted into
  // the shortcut-message template string, to display to the user.
  // For example of shortcut "Alt + left arrow", |shortcut_key_codes| will be
  // {ui::VKEY_LMENU, ui::VKEY_LEFT}. ui::VKEY_LMENU indicates to display a text
  // "Alt" and ui::VKEY_LEFT insidcates to display an icon of "left arrow".
  // Note that the modifier is converted to ui::KeyboardCode so that there is
  // only one enum type to deal with.
  std::vector<ui::KeyboardCode> shortcut_key_codes;
};

}  // namespace keyboard_shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_KEYBOARD_SHORTCUT_ITEM_H_
