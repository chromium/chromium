// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_QUICK_SETTINGS_HELPERS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_QUICK_SETTINGS_HELPERS_H_

#include "chrome/browser/ui/ash/input_method/input_method_menu_item.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"

namespace ash {
namespace input_method {

// Converts a structured input method settings object into a list of menu items
// for the shelf.
std::vector<ui::ime::InputMethodMenuItem> CreateMenuItemsFromQuickSettings(
    const ime::mojom::InputMethodQuickSettings& quick_settings);

// Returns the new quick settings after a menu item is toggled.
// `old_menu_items` is the list of menu items before toggling.
// `toggled_item_key` is the key of the `InputMethodMenuItem` that was toggled.
// `old_menu_items` must come from `CreateMenuItemsFromQuickSettings`.
// It is undefined behavior to call this method with menu items that are not
// from CreateMenuItemsFromQuickSettings.
ime::mojom::InputMethodQuickSettingsPtr GetQuickSettingsAfterToggle(
    const std::vector<ui::ime::InputMethodMenuItem>& old_menu_items,
    std::string toggled_item_key);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_QUICK_SETTINGS_HELPERS_H_
