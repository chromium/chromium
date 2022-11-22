// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_TEST_HOLDING_SPACE_ITEM_VIEWS_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_TEST_HOLDING_SPACE_ITEM_VIEWS_SECTION_H_

#include <memory>

#include "ash/system/holding_space/holding_space_item_views_section.h"

namespace ash {

// Simple implementation of the abstract `HoldingSpaceItemViewsSection` class
// for testing.
class TestHoldingSpaceItemViewsSection : public HoldingSpaceItemViewsSection {
 public:
  TestHoldingSpaceItemViewsSection(HoldingSpaceViewDelegate* view_delegate,
                                   HoldingSpaceSectionId section_id);

 private:
  // HoldingSpaceItemViewsSection:
  std::unique_ptr<views::View> CreateHeader() override;
  std::unique_ptr<views::View> CreateContainer() override;
  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_TEST_HOLDING_SPACE_ITEM_VIEWS_SECTION_H_
