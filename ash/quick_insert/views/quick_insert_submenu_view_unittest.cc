// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_submenu_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace {

constexpr gfx::Rect kDefaultAnchorBounds(200, 100, 100, 10);

using QuickInsertSubmenuViewTest = views::ViewsTestBase;

TEST_F(QuickInsertSubmenuViewTest, GetsTopItem) {
  std::vector<std::unique_ptr<QuickInsertListItemView>> items;
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  auto* top_item_ptr = items.front().get();
  PickerSubmenuView submenu_view(kDefaultAnchorBounds, std::move(items));

  EXPECT_EQ(submenu_view.GetTopItem(), top_item_ptr);
}

TEST_F(QuickInsertSubmenuViewTest, GetsBottomItem) {
  std::vector<std::unique_ptr<QuickInsertListItemView>> items;
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  auto* bottom_item_ptr = items.back().get();
  PickerSubmenuView submenu_view(kDefaultAnchorBounds, std::move(items));

  EXPECT_EQ(submenu_view.GetBottomItem(), bottom_item_ptr);
}

TEST_F(QuickInsertSubmenuViewTest, GetsItemAbove) {
  std::vector<std::unique_ptr<QuickInsertListItemView>> items;
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  auto* top_item_ptr = items.front().get();
  auto* bottom_item_ptr = items.back().get();
  PickerSubmenuView submenu_view(kDefaultAnchorBounds, std::move(items));

  EXPECT_EQ(submenu_view.GetItemAbove(top_item_ptr), nullptr);
  EXPECT_EQ(submenu_view.GetItemAbove(bottom_item_ptr), top_item_ptr);
}

TEST_F(QuickInsertSubmenuViewTest, GetItemBelow) {
  std::vector<std::unique_ptr<QuickInsertListItemView>> items;
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  items.push_back(std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  auto* top_item_ptr = items.front().get();
  auto* bottom_item_ptr = items.back().get();
  PickerSubmenuView submenu_view(kDefaultAnchorBounds, std::move(items));

  EXPECT_EQ(submenu_view.GetItemBelow(top_item_ptr), bottom_item_ptr);
  EXPECT_EQ(submenu_view.GetItemBelow(bottom_item_ptr), nullptr);
}

TEST_F(QuickInsertSubmenuViewTest, TriggersItemCallbackOnPseudoFocusAction) {
  std::vector<std::unique_ptr<QuickInsertListItemView>> items;
  base::test::TestFuture<void> select_item_future;
  items.push_back(std::make_unique<QuickInsertListItemView>(
      select_item_future.GetRepeatingCallback()));
  PickerSubmenuView submenu_view(kDefaultAnchorBounds, std::move(items));

  EXPECT_TRUE(DoPickerPseudoFocusedActionOnView(submenu_view.GetTopItem()));

  EXPECT_TRUE(select_item_future.Wait());
}

}  // namespace
}  // namespace ash
