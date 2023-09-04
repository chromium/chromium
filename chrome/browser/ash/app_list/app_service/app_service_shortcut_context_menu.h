// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_CONTEXT_MENU_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_CONTEXT_MENU_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

class AppContextMenuDelegate;
class AppListControllerDelegate;
class Profile;

namespace ash {
enum class AppListItemContext;
}

// Creates context menus for app items in the app list.
class AppServiceShortcutContextMenu : public app_list::AppContextMenu {
 public:
  AppServiceShortcutContextMenu(app_list::AppContextMenuDelegate* delegate,
                                Profile* profile,
                                const apps::ShortcutId& shortcut_id,
                                AppListControllerDelegate* controller,
                                ash::AppListItemContext item_context);
  ~AppServiceShortcutContextMenu() override;

  AppServiceShortcutContextMenu(const AppServiceShortcutContextMenu&) = delete;
  AppServiceShortcutContextMenu& operator=(
      const AppServiceShortcutContextMenu&) = delete;

  // AppContextMenu overrides:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  const raw_ptr<apps::AppServiceProxy> proxy_;

  apps::ShortcutId shortcut_id_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_SHORTCUT_CONTEXT_MENU_H_
