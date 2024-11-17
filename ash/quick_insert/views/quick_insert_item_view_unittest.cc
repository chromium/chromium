// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_item_view.h"

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(QuickInsertItemViewTest, DefaultIsNormalState) {
  QuickInsertItemView item_view(base::DoNothing());

  EXPECT_EQ(item_view.GetItemState(), QuickInsertItemView::ItemState::kNormal);
}

TEST(QuickInsertItemViewTest, NoBackgroundInNormalState) {
  QuickInsertItemView item_view(base::DoNothing());

  item_view.SetItemState(QuickInsertItemView::ItemState::kNormal);

  EXPECT_FALSE(item_view.background());
}

TEST(QuickInsertItemViewTest, HasBackgroundInPseudoFocusedState) {
  QuickInsertItemView item_view(base::DoNothing());

  item_view.SetItemState(QuickInsertItemView::ItemState::kPseudoFocused);

  EXPECT_TRUE(item_view.background());
}

}  // namespace
}  // namespace ash
