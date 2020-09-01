// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chips_container.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

HoldingSpaceItemChipsContainer::HoldingSpaceItemChipsContainer() {
  using views::GridLayout;
  layout_ = SetLayoutManager(std::make_unique<views::GridLayout>());
  column_set_ = layout_->AddColumnSet(0);

  column_set_->AddColumn(
      GridLayout::Alignment::FILL, GridLayout::Alignment::FILL, 0,
      GridLayout::ColumnSize::kFixed, kHoldingSpaceColumnWidth, 0);
  column_set_->AddPaddingColumn(0, kHoldingSpaceColumnPadding);
  column_set_->AddColumn(
      GridLayout::Alignment::FILL, GridLayout::Alignment::FILL, 0,
      GridLayout::ColumnSize::kFixed, kHoldingSpaceColumnWidth, 0);
}

HoldingSpaceItemChipsContainer::~HoldingSpaceItemChipsContainer() = default;

void HoldingSpaceItemChipsContainer::AddItemChip(HoldingSpaceItem* item) {
  if ((children().size() % 2) == 0) {
    layout_->StartRowWithPadding(0, 0, 0, kHoldingSpaceRowPadding);
  }
  layout_->AddView(std::make_unique<HoldingSpaceItemChipView>(item));
}

const char* HoldingSpaceItemChipsContainer::GetClassName() const {
  return "HoldingSpaceItemChipsContainer";
}

}  // namespace ash