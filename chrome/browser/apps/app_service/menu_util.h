// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"

class Profile;

namespace gfx {
class ImageSkia;
}

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace apps {

// ElementIdentifier associated with the item added by
// PopulateLaunchNewItemFromMenuItem() below.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kLaunchNewMenuItem);

// Adds a command menu item to |menu_items|.
void AddCommandItem(uint32_t command_id,
                    uint32_t string_id,
                    MenuItems& menu_items);

// Adds a separator of the specified type to |menu_items|.
void AddSeparator(ui::MenuSeparatorType separator_type, MenuItems& menu_items);

// Adds a shortcut command menu item to |menu_items|.
void AddShortcutCommandItem(int command_id,
                            const std::string& shortcut_id,
                            const std::string& label,
                            const gfx::ImageSkia& icon,
                            MenuItems& menu_items);

// Adds a LAUNCH_NEW menu item to |menu_items|, and create radio items for the
// submenu.
void CreateOpenNewSubmenu(uint32_t string_id, MenuItems& menu_items);

// Returns true if the open menu item can be added, when |menu_type| is Shelf,
// and the app identified by |app_id| is not running, otherwise returns false.
bool ShouldAddOpenItem(const std::string& app_id,
                       MenuType menu_type,
                       Profile* profile);

// Returns true if the close menu item can be added, when |menu_type| is Shelf,
// and the app identified by |app_id| is running, otherwise returns false.
bool ShouldAddCloseItem(const std::string& app_id,
                        MenuType menu_type,
                        Profile* profile);

// Populates the LAUNCH_NEW menu item to a simple menu model |model| from
// |menu_item|. Also sets initial string id value to |launch_new_string_id|.
void PopulateLaunchNewItemFromMenuItem(const MenuItemPtr& menu_item,
                                       ui::SimpleMenuModel* model,
                                       ui::SimpleMenuModel* submenu,
                                       int* launch_new_string_id);

// Populates the menu item to a simple menu model |model| from menu item
// |menu_item|.
void PopulateItemFromMenuItem(const MenuItemPtr& menu_item,
                              ui::SimpleMenuModel* model,
                              apps::AppShortcutItems* arc_shortcut_items);

// Convert |menu_type| to string. Useful to pass |menu_type| enum as string id.
std::string_view MenuTypeToString(MenuType menu_type);

// Convert |menu_type| string to enum. Useful to pass |menu_type| enum as string
// id.
MenuType MenuTypeFromString(std::string_view menu_type);

// Returns the browser menu items for the given |menu_type|.
MenuItems CreateBrowserMenuItems(const Profile* profile);

ui::ColorId GetColorIdForMenuItemIcon();

// Converts `USE_LAUNCH_TYPE_*` commands to associated string ids.
uint32_t StringIdForUseLaunchTypeCommand(uint32_t command_id);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_
