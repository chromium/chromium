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
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using QuickInsertListItemContainerViewTest = views::ViewsTestBase;

TEST_F(QuickInsertListItemContainerViewTest, GetsTopItem) {
  PickerListItemContainerView container;

  QuickInsertListItemView* top_item = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetTopItem(), top_item);
}

TEST_F(QuickInsertListItemContainerViewTest, EmptyContainerHasNoTopItem) {
  PickerListItemContainerView container;

  EXPECT_EQ(container.GetTopItem(), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, GetsBottomItem) {
  PickerListItemContainerView container;

  container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* bottom_item = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetBottomItem(), bottom_item);
}

TEST_F(QuickInsertListItemContainerViewTest, EmptyContainerHasNoBottomItem) {
  PickerListItemContainerView container;

  EXPECT_EQ(container.GetBottomItem(), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, GetsItemAbove) {
  PickerListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemAbove(item1), nullptr);
  EXPECT_EQ(container.GetItemAbove(item2), item1);
}

TEST_F(QuickInsertListItemContainerViewTest, ItemNotInContainerHasNoItemAbove) {
  PickerListItemContainerView container;

  QuickInsertListItemView item_not_in_container(base::DoNothing());

  EXPECT_EQ(container.GetItemAbove(&item_not_in_container), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, GetsItemBelow) {
  PickerListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemBelow(item1), item2);
  EXPECT_EQ(container.GetItemBelow(item2), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, ItemNotInContainerHasNoItemBelow) {
  PickerListItemContainerView container;

  QuickInsertListItemView item_not_in_container(base::DoNothing());

  EXPECT_EQ(container.GetItemBelow(&item_not_in_container), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, NoItemLeftOf) {
  PickerListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(container.GetItemLeftOf(item2), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, NoItemRightOf) {
  PickerListItemContainerView container;

  QuickInsertListItemView* item1 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));
  QuickInsertListItemView* item2 = container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemRightOf(item1), nullptr);
  EXPECT_EQ(container.GetItemRightOf(item2), nullptr);
}

TEST_F(QuickInsertListItemContainerViewTest, ChildrenHasListItemRole) {
  PickerListItemContainerView container;

  container.AddListItem(
      std::make_unique<QuickInsertListItemView>(base::DoNothing()));

  EXPECT_EQ(container.children()[0]->GetAccessibleRole(),
            ax::mojom::Role::kListItem);
}

}  // namespace
}  // namespace ash
