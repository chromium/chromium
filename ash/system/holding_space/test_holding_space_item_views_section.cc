// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/test_holding_space_item_views_section.h"

#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ui/views/view.h"

namespace ash {

TestHoldingSpaceItemViewsSection::TestHoldingSpaceItemViewsSection(
    HoldingSpaceViewDelegate* view_delegate,
    HoldingSpaceSectionId section_id)
    : HoldingSpaceItemViewsSection(view_delegate, section_id) {}

std::unique_ptr<views::View> TestHoldingSpaceItemViewsSection::CreateHeader() {
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View>
TestHoldingSpaceItemViewsSection::CreateContainer() {
  return std::make_unique<views::View>();
}

std::unique_ptr<HoldingSpaceItemView>
TestHoldingSpaceItemViewsSection::CreateView(const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

}  // namespace ash
