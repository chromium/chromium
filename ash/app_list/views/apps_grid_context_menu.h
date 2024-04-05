// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_GRID_CONTEXT_MENU_H_
#define ASH_APP_LIST_VIEWS_APPS_GRID_CONTEXT_MENU_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuItemView;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace ash {

// This class is the context menu controller used by AppsGridView and
// AppCollectionsPage, responsible for building, running the menu and executing
// the commands.
class ASH_EXPORT AppsGridContextMenu : public ui::SimpleMenuModel::Delegate,
                                       public views::ContextMenuController {
 public:
  // The types of grids where this menu could be shown.
  enum class GridType { kAppsGrid, kAppsCollectionsGrid };

  explicit AppsGridContextMenu(GridType grid_type);
  AppsGridContextMenu(const AppsGridContextMenu&) = delete;
  AppsGridContextMenu& operator=(const AppsGridContextMenu&) = delete;
  ~AppsGridContextMenu() override;

  // Returns true if the apps grid context menu is showing.
  bool IsMenuShowing() const;

  // Closes the context menu if it's showning.
  void Cancel();

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  void set_owner_touch_dragging(bool touch_dragging) {
    owner_touch_dragging_ = touch_dragging;
  }

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
  raw_ptr<views::MenuItemView> root_menu_item_view_ = nullptr;

  // Whether the owner view is currently touch dragging, in which case touch
  // events will be forwarded from the context menu to the owner view (so the
  // view can transition from showing a context menu to item drag).
  bool owner_touch_dragging_ = false;

  // The grid in which this menu is shown.
  GridType grid_type_ = GridType::kAppsGrid;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_GRID_CONTEXT_MENU_H_
