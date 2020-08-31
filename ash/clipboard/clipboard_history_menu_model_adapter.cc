// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include "ash/clipboard/clipboard_history_controller.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

ClipboardHistoryMenuModelAdapter::ClipboardHistoryMenuModelAdapter(
    std::unique_ptr<ui::SimpleMenuModel> model)
    : views::MenuModelAdapter(model.get()), model_(std::move(model)) {}

ClipboardHistoryMenuModelAdapter::~ClipboardHistoryMenuModelAdapter() = default;

void ClipboardHistoryMenuModelAdapter::Run(const gfx::Rect& anchor_rect) {
  DCHECK(!root_view_);
  DCHECK(model_);

  // Start async rendering of HTML, if any exists.
  ClipboardImageModelFactory::Get()->Activate();

  root_view_ = CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      root_view_, views::MenuRunner::CONTEXT_MENU |
                      views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                      views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(
      /*widget_owner=*/nullptr, /*menu_button_controller=*/nullptr, anchor_rect,
      views::MenuAnchorPosition::kBubbleRight, ui::MENU_SOURCE_KEYBOARD);
}

bool ClipboardHistoryMenuModelAdapter::IsRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void ClipboardHistoryMenuModelAdapter::Cancel() {
  DCHECK(menu_runner_);
  menu_runner_->Cancel();
}

base::Optional<int>
ClipboardHistoryMenuModelAdapter::GetSelectedMenuItemCommand() const {
  DCHECK(root_view_);
  auto* menu_item = root_view_->GetMenuController()->GetSelectedMenuItem();
  return menu_item ? base::make_optional(menu_item->GetCommand())
                   : base::nullopt;
}

gfx::Rect ClipboardHistoryMenuModelAdapter::GetMenuBoundsInScreenForTest()
    const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetBoundsInScreen();
}

void ClipboardHistoryMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  ClipboardImageModelFactory::Get()->Deactivate();
}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    int index) {
  const int item_index = model->GetCommandIdAt(index);
  views::MenuItemView* container = menu->AppendMenuItem(item_index);

  // Margins are managed by `ClipboardHistoryItemView`.
  container->SetMargins(/*top_margin=*/0, /*bottom_margin=*/0);

  const auto& items =
      Shell::Get()->clipboard_history_controller()->clipboard_items();

  std::unique_ptr<ClipboardHistoryItemView> item_view =
      ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
          items[item_index], container);
  item_view->Init();
  container->AddChildView(std::move(item_view));

  return container;
}

}  // namespace ash
