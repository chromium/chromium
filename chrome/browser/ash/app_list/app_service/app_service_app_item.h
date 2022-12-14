// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ITEM_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ITEM_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_types.h"

// An app list item provided by the app service.
class AppServiceAppItem : public ChromeAppListItem,
                          public app_list::AppContextMenuDelegate {
 public:
  static const char kItemType[];

  AppServiceAppItem(Profile* profile,
                    AppListModelUpdater* model_updater,
                    const app_list::AppListSyncableService::SyncItem* sync_item,
                    const apps::AppUpdate& app_update);
  AppServiceAppItem(const AppServiceAppItem&) = delete;
  AppServiceAppItem& operator=(const AppServiceAppItem&) = delete;
  ~AppServiceAppItem() override;

  void OnAppUpdate(const apps::AppUpdate& app_update);

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

 private:
  void OnAppUpdate(const apps::AppUpdate& app_update, bool in_constructor);

  // ChromeAppListItem overrides:
  void LoadIcon() override;
  void Activate(int event_flags) override;
  const char* GetItemType() const override;
  void GetContextMenuModel(ash::AppListItemContext item_context,
                           GetMenuModelCallback callback) override;
  app_list::AppContextMenu* GetAppContextMenu() override;

  // Resets the `is_new_install` property and records metrics.
  void ResetIsNewInstall();

  void Launch(int event_flags, apps::LaunchSource launch_source);

  void CallLoadIcon(bool allow_placeholder_icon);
  void OnLoadIcon(apps::IconValuePtr icon_value);

  const apps::AppType app_type_;
  const base::TimeTicks creation_time_;  // When this object was created.
  bool is_platform_app_ = false;

  std::unique_ptr<app_list::AppContextMenu> context_menu_;

  base::WeakPtrFactory<AppServiceAppItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SERVICE_APP_SERVICE_APP_ITEM_H_
