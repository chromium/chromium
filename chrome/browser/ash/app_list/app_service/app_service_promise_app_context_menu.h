// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_CONTEXT_MENU_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/app_context_menu.h"

class AppContextMenuDelegate;
class AppListControllerDelegate;
class Profile;

namespace apps {
class PackageId;
}

namespace ash {
enum class AppListItemContext;
}

// Creates context menus for app items in the app list.
class AppServicePromiseAppContextMenu : public app_list::AppContextMenu {
 public:
  AppServicePromiseAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                                  Profile* profile,
                                  const apps::PackageId& package_id,
                                  AppListControllerDelegate* controller,
                                  ash::AppListItemContext item_context);
  ~AppServicePromiseAppContextMenu() override;

  AppServicePromiseAppContextMenu(const AppServicePromiseAppContextMenu&) =
      delete;
  AppServicePromiseAppContextMenu& operator=(
      const AppServicePromiseAppContextMenu&) = delete;

  // AppContextMenu overrides:
  void GetMenuModel(GetMenuModelCallback callback) override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  const raw_ptr<apps::AppServiceProxy> proxy_;

  const raw_ref<const apps::PackageId> package_id_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_PROMISE_APP_CONTEXT_MENU_H_
