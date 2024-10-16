// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"

#include "ash/quick_insert/views/quick_insert_badge_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

using QuickInsertPseudoFocusTest = views::ViewsTestBase;

TEST_F(QuickInsertPseudoFocusTest,
       ApplyPseudoFocusToListItemUpdatesItemStateAndBadge) {
  QuickInsertListItemView item_view(base::DoNothing());

  ApplyPickerPseudoFocusToView(&item_view);

  EXPECT_EQ(item_view.GetItemState(),
            QuickInsertItemView::ItemState::kPseudoFocused);
  EXPECT_TRUE(item_view.trailing_badge_for_testing().GetVisible());
}

TEST_F(QuickInsertPseudoFocusTest,
       RemovePseudoFocusFromListItemUpdatesItemStateAndBadge) {
  QuickInsertListItemView item_view(base::DoNothing());
  ApplyPickerPseudoFocusToView(&item_view);

  RemovePickerPseudoFocusFromView(&item_view);

  EXPECT_EQ(item_view.GetItemState(), QuickInsertItemView::ItemState::kNormal);
  EXPECT_FALSE(item_view.trailing_badge_for_testing().GetVisible());
}

}  // namespace
}  // namespace ash
