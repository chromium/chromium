// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_list_item_container_view.h"

#include <memory>

#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using QuickInsertListItemContainerViewTest = views::ViewsTestBase;

TEST_F(QuickInsertListItemContainerViewTest, GetsTopItem) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView* top_item = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetTopItem(), top_item);
}

TEST_F(QuickInsertListItemContainerViewTest, EmptyContainerHasNoTopItem) {
  QuickInsertListItemContainerView container;

  EXPECT_EQ(container.GetTopItem(), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, GetsBottomItem) {
  QuickInsertListItemContainerView container;

  container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* bottom_item = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetBottomItem(), bottom_item);
}

TEST_F(QuickInsertListItemContainerViewTest, EmptyContainerHasNoBottomItem) {
  QuickInsertListItemContainerView container;

  EXPECT_EQ(container.GetBottomItem(), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, GetsItemAbove) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemAbove(item1), nullptr);
  EXPECT_EQ(container.GetItemAbove(item2), item1);
}

TEST_F(QuickInsertListItemContainerViewTest, ItemNotInContainerHasNoItemAbove) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView item_not_in_container(base::DoNothing());

  EXPECT_EQ(container.GetItemAbove(&item_not_in_container), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, GetsItemBelow) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemBelow(item1), item2);
  EXPECT_EQ(container.GetItemBelow(item2), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, ItemNotInContainerHasNoItemBelow) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView item_not_in_container(base::DoNothing());

  EXPECT_EQ(container.GetItemBelow(&item_not_in_container), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, NoItemLeftOf) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(container.GetItemLeftOf(item2), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, NoItemRightOf) {
  QuickInsertListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemRightOf(item1), nullptr);
  EXPECT_EQ(container.GetItemRightOf(item2), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, ChildrenHasListItemRole) {
  QuickInsertListItemContainerView container;

  container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  ui::AXNodeData node_data;
  container.children()[0]->GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kListItem);
}

}  // namespace
}  // namespace ash
