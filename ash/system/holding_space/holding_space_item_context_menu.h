// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CONTEXT_MENU_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CONTEXT_MENU_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class MenuRunner;
}  // namespace views

namespace ash {

// This class handles creation of the context menu view and handling
// commands available in the menu.
class ASH_EXPORT HoldingSpaceItemContextMenu
    : public views::ContextMenuController,
      public ui::SimpleMenuModel::Delegate {
 public:
  HoldingSpaceItemContextMenu();
  HoldingSpaceItemContextMenu(const HoldingSpaceItemContextMenu&) = delete;
  HoldingSpaceItemContextMenu& operator=(const HoldingSpaceItemContextMenu&) =
      delete;
  ~HoldingSpaceItemContextMenu() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  ui::SimpleMenuModel* BuildMenuModel();

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CONTEXT_MENU_H_
