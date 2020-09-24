// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_view_delegate.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "base/bind.h"
#include "net/base/mime_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// It is expected that all `HoldingSpaceItemView`s share the same delegate in
// order to support multiple selections. We cache the singleton `instance` in
// order to enforce this requirement.
HoldingSpaceItemViewDelegate* instance = nullptr;

// Helpers ---------------------------------------------------------------------

// Attempts to open the specified holding space `item`.
void OpenItem(const HoldingSpaceItem& item) {
  HoldingSpaceController::Get()->client()->OpenItem(item, base::DoNothing());
}

}  // namespace

// HoldingSpaceItemViewDelegate ------------------------------------------------

HoldingSpaceItemViewDelegate::HoldingSpaceItemViewDelegate() {
  DCHECK_EQ(nullptr, instance);
  instance = this;
}

HoldingSpaceItemViewDelegate::~HoldingSpaceItemViewDelegate() {
  DCHECK_EQ(instance, this);
  instance = nullptr;
}

// TODO(dmblack): Implement multiple selection.
void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewGestureEvent(
    HoldingSpaceItemView* view,
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP)
    OpenItem(*view->item());
}

// TODO(dmblack): Handle multiple selection.
bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewKeyPressed(
    HoldingSpaceItemView* view,
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    OpenItem(*view->item());
    return true;
  }
  return false;
}

// TODO(dmblack): Handle multiple selection.
bool HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewMousePressed(
    HoldingSpaceItemView* view,
    const ui::MouseEvent& event) {
  if (event.flags() & ui::EF_IS_DOUBLE_CLICK) {
    OpenItem(*view->item());
    return true;
  }
  return false;
}

void HoldingSpaceItemViewDelegate::OnHoldingSpaceItemViewDestroyed(
    HoldingSpaceItemView* view) {
  selected_views_by_item_id_.erase(view->item()->id());
}

// TODO(dmblack): Handle multiple selection.
void HoldingSpaceItemViewDelegate::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  selected_views_by_item_id_.clear();

  HoldingSpaceItemView* selected_view = HoldingSpaceItemView::Cast(source);
  selected_views_by_item_id_[selected_view->item()->id()] = selected_view;

  int run_types = views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                  views::MenuRunner::CONTEXT_MENU |
                  views::MenuRunner::FIXED_ANCHOR;

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(BuildMenuModel(), run_types);

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), nullptr /*button_controller*/,
      source->GetBoundsInScreen(), views::MenuAnchorPosition::kBubbleRight,
      source_type);
}

// TODO(dmblack): Handle multiple selection.
bool HoldingSpaceItemViewDelegate::CanStartDragForView(
    views::View* sender,
    const gfx::Point& press_pt,
    const gfx::Point& current_pt) {
  selected_views_by_item_id_.clear();

  HoldingSpaceItemView* selected_view = HoldingSpaceItemView::Cast(sender);
  selected_views_by_item_id_[selected_view->item()->id()] = selected_view;

  return true;
}

int HoldingSpaceItemViewDelegate::GetDragOperationsForView(
    views::View* sender,
    const gfx::Point& press_pt) {
  return ui::DragDropTypes::DRAG_COPY;
}

// TODO(dmblack): Handle multiple selection.
void HoldingSpaceItemViewDelegate::WriteDragDataForView(
    views::View* sender,
    const gfx::Point& press_pt,
    ui::OSExchangeData* data) {
  DCHECK_EQ(1u, selected_views_by_item_id_.size());
  auto* selected_view = selected_views_by_item_id_.begin()->second;
  data->SetFilename(selected_view->item()->file_path());
}

// TODO(dmblack): Handle multiple selection.
void HoldingSpaceItemViewDelegate::ExecuteCommand(int command_id,
                                                  int event_flags) {
  DCHECK_EQ(1u, selected_views_by_item_id_.size());
  auto* selected_view = selected_views_by_item_id_.begin()->second;

  switch (command_id) {
    case HoldingSpaceCommandId::kCopyImageToClipboard:
      HoldingSpaceController::Get()->client()->CopyImageToClipboard(
          *selected_view->item(), base::DoNothing());
      break;
    case HoldingSpaceCommandId::kPinItem:
      HoldingSpaceController::Get()->client()->PinItem(*selected_view->item());
      break;
    case HoldingSpaceCommandId::kShowInFolder:
      HoldingSpaceController::Get()->client()->ShowItemInFolder(
          *selected_view->item(), base::DoNothing());
      break;
    case HoldingSpaceCommandId::kUnpinItem:
      HoldingSpaceController::Get()->client()->UnpinItem(
          *selected_view->item());
      break;
  }
}

// TODO(dmblack): Handle multiple selection.
ui::SimpleMenuModel* HoldingSpaceItemViewDelegate::BuildMenuModel() {
  DCHECK_EQ(1u, selected_views_by_item_id_.size());
  auto* selected_view = selected_views_by_item_id_.begin()->second;

  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItemWithIcon(
      HoldingSpaceCommandId::kShowInFolder,
      l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_IN_FOLDER),
      ui::ImageModel::FromVectorIcon(kFolderIcon));

  std::string mime_type;
  const bool is_image = net::GetMimeTypeFromFile(
                            selected_view->item()->file_path(), &mime_type) &&
                        net::MatchesMimeType(kMimeTypeImage, mime_type);

  if (is_image) {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kCopyImageToClipboard,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_COPY_IMAGE_TO_CLIPBOARD),
        ui::ImageModel::FromVectorIcon(kCopyIcon));
  }

  const bool is_pinned = HoldingSpaceController::Get()->model()->GetItem(
      HoldingSpaceItem::GetFileBackedItemId(
          HoldingSpaceItem::Type::kPinnedFile,
          selected_view->item()->file_path()));

  if (!is_pinned) {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kPinItem,
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PIN),
        ui::ImageModel::FromVectorIcon(views::kPinIcon));
  } else {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kUnpinItem,
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_UNPIN),
        ui::ImageModel::FromVectorIcon(views::kUnpinIcon));
  }

  return context_menu_model_.get();
}

}  // namespace ash
