// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CONTEXT_MENU_H_
#define ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CONTEXT_MENU_H_

#include <memory>

#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuRunner;
}  // namespace views

namespace ash {

// A helper class to manage the context menu of the persistent desks bar.
class PersistentDesksBarContextMenu : public views::ContextMenuController,
                                      public ui::SimpleMenuModel::Delegate {
 public:
  // Commands of the context menu.
  enum class CommandId {
    kFeedBack,
    kShowOrHideBar,
  };

  explicit PersistentDesksBarContextMenu(
      base::RepeatingClosure on_menu_closed_callback);
  PersistentDesksBarContextMenu(const PersistentDesksBarContextMenu&) = delete;
  PersistentDesksBarContextMenu& operator=(
      const PersistentDesksBarContextMenu&) = delete;
  ~PersistentDesksBarContextMenu() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuClosed(ui::SimpleMenuModel* menu) override;

 private:
  ui::SimpleMenuModel* BuildMenuModel();

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  base::RepeatingClosure on_menu_closed_callback_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CONTEXT_MENU_H_
