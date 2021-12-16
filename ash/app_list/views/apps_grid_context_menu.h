// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_CONTEXT_MENU_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_CONTEXT_MENU_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuItemView;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace ash {

// This class is the context menu controller used by AppsGridView, responsible
// for building, running the menu and executing the commands.
class ASH_EXPORT AppsGridContextMenu : public ui::SimpleMenuModel::Delegate,
                                       public views::ContextMenuController {
 public:
  // List of command id used in apps grid context menu.
  enum AppsGridCommandId {
    // Command Id that contains a submenu with app name reorder options.
    kReorderByName,

    // Command that will sort the name in alphabetical order.
    kReorderByNameAlphabetical,

    // Command that will sort the name in reverse alphabetical order.
    kReorderByNameReverseAlphabetical,

    // Command that will sort by icon color in rainbow order.
    kReorderByColor
  };

  AppsGridContextMenu();
  AppsGridContextMenu(const AppsGridContextMenu&) = delete;
  AppsGridContextMenu& operator=(const AppsGridContextMenu&) = delete;
  ~AppsGridContextMenu() override;

  // Returns true if the apps grid context menu is showing.
  bool IsMenuShowing() const;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  views::MenuItemView* root_menu_item_view() const {
    return root_menu_item_view_;
  }

 private:
  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Builds and saves a SimpleMenuModel to `context_menu_model_`;
  void BuildMenuModel();

  // Called when the context menu is closed. Used as a callback for
  // `menu_model_adapter_`.
  void OnMenuClosed();

  // The context menu model and its adapter for AppsGridView.
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // The menu runner that is responsible to run the menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The root menu item view of `context_menu_model_`. Cached for testing.
  views::MenuItemView* root_menu_item_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_CONTEXT_MENU_H_
