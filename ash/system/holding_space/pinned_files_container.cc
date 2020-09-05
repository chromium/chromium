// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_container.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

PinnedFilesContainer::PinnedFilesContainer() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding));

  auto* title_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_TITLE)));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::HOLDING_SPACE_TITLE,
                           true /* use_unified_theme */);
  style.SetupLabel(title_label);
  title_label->SetPaintToLayer();
  title_label->layer()->SetFillsBoundsOpaquely(false);

  item_chips_container_ =
      AddChildView(std::make_unique<HoldingSpaceItemChipsContainer>());

  // TODO(crbug.com/1125254): Populate containers if and when holding space
  // model is attached, below is a temporary solution.
  for (const auto& item : HoldingSpaceController::Get()->model()->items()) {
    if (item->type() == HoldingSpaceItem::Type::kPinnedFile)
      item_chips_container_->AddItemChip(item.get());
  }
}

PinnedFilesContainer::~PinnedFilesContainer() = default;

const char* PinnedFilesContainer::GetClassName() const {
  return "PinnedFilesContainer";
}

}  // namespace ash
