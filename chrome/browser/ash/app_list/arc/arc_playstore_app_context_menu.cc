// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_playstore_app_context_menu.h"

#include <string>

#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/grit/generated_resources.h"

ArcPlayStoreAppContextMenu::ArcPlayStoreAppContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    AppListControllerDelegate* controller)
    : app_list::AppContextMenu(delegate, profile, std::string(), controller) {}

ArcPlayStoreAppContextMenu::~ArcPlayStoreAppContextMenu() = default;

void ArcPlayStoreAppContextMenu::BuildMenu(ui::SimpleMenuModel* menu_model) {
  // App Info item.
  menu_model->AddItemWithStringId(ash::INSTALL,
                                  IDS_APP_CONTEXT_MENU_INSTALL_ARC);
}

bool ArcPlayStoreAppContextMenu::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case ash::INSTALL:
      return true;
    default:
      return app_list::AppContextMenu::IsCommandIdEnabled(command_id);
  }
}

void ArcPlayStoreAppContextMenu::ExecuteCommand(int command_id,
                                                int event_flags) {
  switch (command_id) {
    case ash::INSTALL:
      delegate()->ExecuteLaunchCommand(event_flags);
      break;
    default:
      app_list::AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}
