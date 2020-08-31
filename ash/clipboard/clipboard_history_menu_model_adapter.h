// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_

#include <memory>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace ash {

class ClipboardHistoryItem;
class ClipboardHistory;

// Used to show the clipboard history menu, which holds the last few things
// copied.
class ClipboardHistoryMenuModelAdapter : views::MenuModelAdapter {
 public:
  static std::unique_ptr<ClipboardHistoryMenuModelAdapter> Create(
      ui::SimpleMenuModel::Delegate* delegate,
      const ClipboardHistory* clipboard_history);

  ClipboardHistoryMenuModelAdapter(const ClipboardHistoryMenuModelAdapter&) =
      delete;
  ClipboardHistoryMenuModelAdapter& operator=(
      const ClipboardHistoryMenuModelAdapter&) = delete;
  ~ClipboardHistoryMenuModelAdapter() override;

  // Shows the menu, anchored below |anchor_rect|.
  void Run(const gfx::Rect& anchor_rect);

  // Returns if the menu is currently running.
  bool IsRunning() const;

  // Hides and cancels the menu.
  void Cancel();

  // Returns the command of the currently selected menu item. If no menu item is
  // currently selected, returns |base::nullopt|.
  base::Optional<int> GetSelectedMenuItemCommand() const;

  // Returns the item mapped by `command_id` in `item_snapshots_`.
  const ClipboardHistoryItem& GetItemFromCommandId(int command_id) const;

  // Returns the count of menu items.
  int GetMenuItemsCount() const;

  // Remove the menu item specified by `command_id`.
  void RemoveMenuItemWithCommandId(int command_id);

  // Returns menu bounds in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

 private:
  ClipboardHistoryMenuModelAdapter(std::unique_ptr<ui::SimpleMenuModel> model,
                                   const ClipboardHistory* clipboard_history);

  // views::MenuModelAdapter:
  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      int index) override;

  // The model which holds the contents of the menu.
  std::unique_ptr<ui::SimpleMenuModel> const model_;
  // The root MenuItemView which contains all child MenuItemViews. Owned by
  // |menu_runner_|.
  views::MenuItemView* root_view_ = nullptr;
  // Responsible for showing |root_view_|.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The mapping between the command ids and items that are copied from
  // `clipboard_history_` when the menu is created. It is used to solve the
  // possible inconsistency between the menu model data and the clipboard
  // history data. For example, a new item is added to `clipboard_history_`
  // while the menu is showing.
  std::map<int, ClipboardHistoryItem> item_snapshots_;

  const ClipboardHistory* const clipboard_history_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
