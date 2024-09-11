// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/coral_item_remover.h"

#include "ash/public/cpp/coral_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using CoralItemRemoverTest = ::testing::Test;

TEST_F(CoralItemRemoverTest, FilterContent) {
  CoralItemRemover coral_item_remover_;
  coral_util::ContentItem item0 =
      coral_util::TabData("tab 0 title", "tab 0 source");
  coral_util::ContentItem item1 =
      coral_util::TabData("tab 1 title", "tab 1 source");
  coral_util::ContentItem item2 = coral_util::TabData("app 0 id", "app 0 name");
  coral_util::ContentItem item3 = coral_util::TabData("app 1 id", "app 1 name");
  std::vector<coral_util::ContentItem> tab_items = {item0, item1, item2, item3};

  // Filter `tab_items` before any items are removed. The list should remain
  // unchanged.
  coral_item_remover_.FilterRemovedItems(&tab_items);
  ASSERT_EQ(4u, tab_items.size());

  // Remove `item2`, and filter it from the list of tabs.
  coral_item_remover_.RemoveItem(item2);
  coral_item_remover_.FilterRemovedItems(&tab_items);

  // Check that `item2` is filtered out.
  ASSERT_EQ(3u, tab_items.size());
  EXPECT_EQ(tab_items, std::vector({item0, item1, item3}));

  // Remove `item1`, and filter it from the list of tabs.
  coral_item_remover_.RemoveItem(item1);
  coral_item_remover_.FilterRemovedItems(&tab_items);

  // Check that `item1` is filtered out.
  ASSERT_EQ(2u, tab_items.size());
  EXPECT_EQ(tab_items, std::vector({item0, item3}));
}

}  // namespace ash
