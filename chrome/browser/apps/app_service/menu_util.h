// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
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

using GetVectorIconCallback =
    base::OnceCallback<const gfx::VectorIcon&(int command_id, int string_id)>;

// Adds a command menu item to |menu_items|.
void AddCommandItem(uint32_t command_id,
                    uint32_t string_id,
                    apps::mojom::MenuItemsPtr* menu_items);

// Adds a radio menu item to |menu_items|.
void AddRadioItem(uint32_t command_id,
                  uint32_t string_id,
                  int group_id,
                  apps::mojom::MenuItemsPtr* menu_items);

// Adds a separator of the specified type to |menu_items|.
void AddSeparator(ui::MenuSeparatorType separator_type,
                  apps::mojom::MenuItemsPtr* menu_items);

// Adds a shortcut command menu item to |menu_items|.
void AddShortcutCommandItem(int command_id,
                            const std::string& shortcut_id,
                            const std::string& label,
                            const gfx::ImageSkia& icon,
                            apps::mojom::MenuItemsPtr* menu_items);

// Adds a LAUNCH_NEW menu item to |menu_items|, and create radio items for the
// submenu.
void CreateOpenNewSubmenu(apps::mojom::MenuType menu_type,
                          uint32_t string_id,
                          apps::mojom::MenuItemsPtr* menu_items);

// Returns true if the open menu item can be added, when |menu_type| is Shelf,
// and the app identified by |app_id| is not running, otherwise returns false.
bool ShouldAddOpenItem(const std::string& app_id,
                       apps::mojom::MenuType menu_type,
                       Profile* profile);

// Returns true if the close menu item can be added, when |menu_type| is Shelf,
// and the app identified by |app_id| is running, otherwise returns false.
bool ShouldAddCloseItem(const std::string& app_id,
                        apps::mojom::MenuType menu_type,
                        Profile* profile);

// Populates the LAUNCH_NEW menu item to a simple menu model |model| from mojo
// menu items |menu_items|. Returns true if the LAUNCH_NEW menu item is added to
// |model|, otherwise returns false.
bool PopulateNewItemFromMojoMenuItems(
    const std::vector<apps::mojom::MenuItemPtr>& menu_items,
    ui::SimpleMenuModel* model,
    ui::SimpleMenuModel* submenu,
    GetVectorIconCallback get_vector_icon);

// Populates the menu item to a simple menu model |model| from mojo
// menu items |menu_items|.
void PopulateItemFromMojoMenuItems(apps::mojom::MenuItemPtr menu_item,
                                   ui::SimpleMenuModel* model,
                                   apps::AppShortcutItems* arc_shortcut_items);

// Convert |menu_type| to string. Useful to pass |menu_type| enum as string id.
base::StringPiece MenuTypeToString(apps::mojom::MenuType menu_type);

// Convert |menu_type| string to enum. Useful to pass |menu_type| enum as string
// id.
apps::mojom::MenuType MenuTypeFromString(base::StringPiece menu_type);

// A size of square shortcut menu item icons in the context menu.
constexpr int kAppShortcutIconSizeDip = 32;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_MENU_UTIL_H_
