// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
#define ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_

#include <memory>

#include "base/optional.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class MenuItemView;
class MenuRunner;
}  // namespace views

namespace ash {

// Used to show the clipboard history menu, which holds the last few things
// copied.
class ClipboardHistoryMenuModelAdapter : views::MenuModelAdapter {
 public:
  explicit ClipboardHistoryMenuModelAdapter(
      std::unique_ptr<ui::SimpleMenuModel> model);
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

  // Returns menu bounds in screen coordinates.
  gfx::Rect GetMenuBoundsInScreenForTest() const;

 private:
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
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_HISTORY_MENU_MODEL_ADAPTER_H_
