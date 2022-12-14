// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PLAYSTORE_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PLAYSTORE_APP_CONTEXT_MENU_H_

#include "chrome/browser/ash/app_list/app_context_menu.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {
class AppContextMenuDelegate;
}

class ArcPlayStoreAppContextMenu : public app_list::AppContextMenu {
 public:
  ArcPlayStoreAppContextMenu(app_list::AppContextMenuDelegate* delegate,
                             Profile* profile,
                             AppListControllerDelegate* controller);

  ArcPlayStoreAppContextMenu(const ArcPlayStoreAppContextMenu&) = delete;
  ArcPlayStoreAppContextMenu& operator=(const ArcPlayStoreAppContextMenu&) =
      delete;

  ~ArcPlayStoreAppContextMenu() override;

  // AppListContextMenu overrides:
  void BuildMenu(ui::SimpleMenuModel* menu_model) override;

  // ui::SimpleMenuModel::Delegate overrides:
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsCommandIdEnabled(int command_id) const override;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PLAYSTORE_APP_CONTEXT_MENU_H_
