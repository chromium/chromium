// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_context_menu.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "base/bind.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {

HoldingSpaceItemContextMenu::HoldingSpaceItemContextMenu(
    const HoldingSpaceItem* item)
    : item_(item) {}

HoldingSpaceItemContextMenu::~HoldingSpaceItemContextMenu() = default;

void HoldingSpaceItemContextMenu::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
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

void HoldingSpaceItemContextMenu::ExecuteCommand(int command_id,
                                                 int event_flags) {
  switch (command_id) {
    case HoldingSpaceCommandId::kCopyImageToClipboard:
      HoldingSpaceController::Get()->client()->CopyImageToClipboard(
          *item_, base::DoNothing());
      break;
    case HoldingSpaceCommandId::kPinItem:
      HoldingSpaceController::Get()->client()->PinItem(*item_);
      break;
    case HoldingSpaceCommandId::kShowInFolder:
      HoldingSpaceController::Get()->client()->ShowItemInFolder(
          *item_, base::DoNothing());
      break;
    case HoldingSpaceCommandId::kUnpinItem:
      HoldingSpaceController::Get()->client()->UnpinItem(*item_);
      break;
  }
}

ui::SimpleMenuModel* HoldingSpaceItemContextMenu::BuildMenuModel() {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItemWithIcon(
      HoldingSpaceCommandId::kShowInFolder,
      l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_IN_FOLDER),
      ui::ImageModel::FromVectorIcon(kFolderIcon));

  std::string mime_type;
  const bool is_image =
      net::GetMimeTypeFromFile(item_->file_path(), &mime_type) &&
      net::MatchesMimeType(kMimeTypeImage, mime_type);

  if (is_image) {
    context_menu_model_->AddItemWithIcon(
        HoldingSpaceCommandId::kCopyImageToClipboard,
        l10n_util::GetStringUTF16(
            IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_COPY_IMAGE_TO_CLIPBOARD),
        ui::ImageModel::FromVectorIcon(kCopyIcon));
  }

  const bool is_pinned = HoldingSpaceController::Get()->model()->GetItem(
      HoldingSpaceItem::GetFileBackedItemId(HoldingSpaceItem::Type::kPinnedFile,
                                            item_->file_path()));
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
