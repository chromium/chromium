// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_context_menu.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace ash {

HoldingSpaceItemContextMenu::HoldingSpaceItemContextMenu() = default;

HoldingSpaceItemContextMenu::~HoldingSpaceItemContextMenu() = default;

ui::SimpleMenuModel* HoldingSpaceItemContextMenu::BuildMenuModel() {
  context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  context_menu_model_->AddItem(
      HoldingSpaceCommandId::kShowInFolder,
      l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_SHOW_IN_FOLDER));
  context_menu_model_->AddItem(
      HoldingSpaceCommandId::kCopyToClipboard,
      l10n_util::GetStringUTF16(
          IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_COPY_TO_CLIPBOARD));
  context_menu_model_->AddItem(
      HoldingSpaceCommandId::kTogglePinItem,
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_CONTEXT_MENU_PIN));

  return context_menu_model_.get();
}

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
    case HoldingSpaceCommandId::kCopyToClipboard:
      // TODO(crbug.com/1127240): Hookup API for copy to clipboard
      break;
    case HoldingSpaceCommandId::kShowInFolder:
      // TODO(crbug.com/1127240): Hookup API for show in folder
      break;
    case HoldingSpaceCommandId::kTogglePinItem:
      // TODO(crbug.com/1127240): Hookup API for toggling pin
      break;
  }
}

}  // namespace ash
