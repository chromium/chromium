// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/app_shortcut_item.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/menu.h"

class AppContextMenuDelegate;
class AppListControllerDelegate;
class Profile;
class StandaloneBrowserExtensionAppContextMenu;

namespace ash {
enum class AppListItemContext;
}

namespace extensions {
class ContextMenuMatcher;
}

// Creates context menus for app items in the app list.
class AppServiceContextMenu : public app_list::AppContextMenu {
 public:
  AppServiceContextMenu(app_list::AppContextMenuDelegate* delegate,
                        Profile* profile,
                        const std::string& app_id,
                        AppListControllerDelegate* controller,
                        ash::AppListItemContext item_context);
  ~AppServiceContextMenu() override;

  AppServiceContextMenu(const AppServiceContextMenu&) = delete;
  AppServiceContextMenu& operator=(const AppServiceContextMenu&) = delete;

  // AppContextMenu overrides:
  ui::ImageModel GetIconForCommandId(int command_id) const override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;

 private:
  void OnGetMenuModel(GetMenuModelCallback callback,
                      apps::MenuItems menu_items);

  // Build additional extension app menu items.
  void BuildExtensionAppShortcutsMenu(ui::SimpleMenuModel* menu_model);

  void ShowAppInfo();

  void SetLaunchType(int command_id);

  void ExecutePublisherContextMenuCommand(int command_id);

  apps::AppType app_type_ = apps::AppType::kUnknown;
  bool is_platform_app_ = false;

  // The SimpleMenuModel used to hold the submenu items.
  std::unique_ptr<ui::SimpleMenuModel> submenu_;

  std::unique_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  // This member holds all logic for context menus associated with standalone
  // browser extension apps.
  std::unique_ptr<StandaloneBrowserExtensionAppContextMenu>
      standalone_browser_extension_menu_;

  // Caches the app shortcut items.
  std::unique_ptr<apps::AppShortcutItems> app_shortcut_items_;

  const raw_ptr<apps::AppServiceProxy> proxy_;

  // String id for the `LAUNCH_NEW` menu item tracked so the menu icon and label
  // can be changed dynamically after the app launch type changes using the
  // launch new item submenu.
  int launch_new_string_id_ = 0;

  base::WeakPtrFactory<AppServiceContextMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_CONTEXT_MENU_H_
