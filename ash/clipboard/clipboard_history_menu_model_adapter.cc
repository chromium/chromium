// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

// static
std::unique_ptr<ClipboardHistoryMenuModelAdapter>
ClipboardHistoryMenuModelAdapter::Create(
    ui::SimpleMenuModel::Delegate* delegate,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history,
    const ClipboardHistoryResourceManager* resource_manager) {
  return base::WrapUnique(new ClipboardHistoryMenuModelAdapter(
      std::make_unique<ui::SimpleMenuModel>(delegate),
      std::move(menu_closed_callback), clipboard_history, resource_manager));
}

ClipboardHistoryMenuModelAdapter::~ClipboardHistoryMenuModelAdapter() = default;

void ClipboardHistoryMenuModelAdapter::Run(const gfx::Rect& anchor_rect) {
  DCHECK(!root_view_);
  DCHECK(model_);
  DCHECK(item_snapshots_.empty());

  int command_id = ClipboardHistoryUtil::kFirstItemCommandId;
  for (const auto& item : clipboard_history_->GetItems()) {
    model_->AddItem(command_id, base::string16());
    item_snapshots_.emplace(command_id, item);
    ++command_id;
  }

  // Enable the command execution through the model delegate.
  model_->AddItem(ClipboardHistoryUtil::kDeleteCommandId, base::string16());

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

  // `root_view_` may be selected if no menu item is under selection.
  auto* menu_item = root_view_->GetMenuController()->GetSelectedMenuItem();
  return menu_item && menu_item != root_view_
             ? base::make_optional(menu_item->GetCommand())
             : base::nullopt;
}

const ClipboardHistoryItem&
ClipboardHistoryMenuModelAdapter::GetItemFromCommandId(int command_id) const {
  auto iter = item_snapshots_.find(command_id);
  DCHECK(iter != item_snapshots_.cend());
  return iter->second;
}

int ClipboardHistoryMenuModelAdapter::GetMenuItemsCount() const {
  return root_view_->GetSubmenu()->GetRowCount();
}

void ClipboardHistoryMenuModelAdapter::RemoveMenuItemWithCommandId(
    int command_id) {
  model_->RemoveItemAt(model_->GetIndexOfCommandId(command_id));
  root_view_->RemoveMenuItem(root_view_->GetMenuItemByID(command_id));
  root_view_->ChildrenChanged();
}

gfx::Rect ClipboardHistoryMenuModelAdapter::GetMenuBoundsInScreenForTest()
    const {
  DCHECK(root_view_);
  return root_view_->GetSubmenu()->GetBoundsInScreen();
}

ClipboardHistoryMenuModelAdapter::ClipboardHistoryMenuModelAdapter(
    std::unique_ptr<ui::SimpleMenuModel> model,
    base::RepeatingClosure menu_closed_callback,
    const ClipboardHistory* clipboard_history,
    const ClipboardHistoryResourceManager* resource_manager)
    : views::MenuModelAdapter(model.get(), std::move(menu_closed_callback)),
      model_(std::move(model)),
      clipboard_history_(clipboard_history),
      resource_manager_(resource_manager) {}

views::MenuItemView* ClipboardHistoryMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    int index) {
  const int command_id = model->GetCommandIdAt(index);

  // Do not create the view for the deletion command.
  if (command_id == ClipboardHistoryUtil::kDeleteCommandId)
    return nullptr;

  views::MenuItemView* container = menu->AppendMenuItem(command_id);

  // Margins are managed by `ClipboardHistoryItemView`.
  container->SetMargins(/*top_margin=*/0, /*bottom_margin=*/0);

  std::unique_ptr<ClipboardHistoryItemView> item_view =
      ClipboardHistoryItemView::CreateFromClipboardHistoryItem(
          GetItemFromCommandId(command_id), *resource_manager_, container);
  item_view->Init();
  container->AddChildView(std::move(item_view));

  return container;
}

void ClipboardHistoryMenuModelAdapter::OnMenuClosed(views::MenuItemView* menu) {
  ClipboardImageModelFactory::Get()->Deactivate();
  views::MenuModelAdapter::OnMenuClosed(menu);
}

}  // namespace ash
