// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_pseudo_focus.h"

#include "ash/picker/views/picker_badge_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

using PickerPseudoFocusTest = views::ViewsTestBase;

TEST_F(PickerPseudoFocusTest,
       ApplyPseudoFocusToListItemUpdatesItemStateAndBadge) {
  PickerListItemView item_view(base::DoNothing());

  ApplyPickerPseudoFocusToView(&item_view);

  EXPECT_EQ(item_view.GetItemState(),
            PickerItemView::ItemState::kPseudoFocused);
  EXPECT_TRUE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(PickerPseudoFocusTest,
       RemovePseudoFocusFromListItemUpdatesItemStateAndBadge) {
  PickerListItemView item_view(base::DoNothing());
  ApplyPickerPseudoFocusToView(&item_view);

  RemovePickerPseudoFocusFromView(&item_view);

  EXPECT_EQ(item_view.GetItemState(), PickerItemView::ItemState::kNormal);
  EXPECT_FALSE(item_view.trailing_badge_for_testing().GetVisible());
}

}  // namespace
}  // namespace ash
