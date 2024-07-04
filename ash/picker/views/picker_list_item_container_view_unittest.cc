// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_container_view.h"

#include <memory>

#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using PickerListItemContainerViewTest = views::ViewsTestBase;

TEST_F(PickerListItemContainerViewTest, GetsTopItem) {
  PickerListItemContainerView container;

  PickerListItemView* top_item = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetTopItem(), top_item);
}

TEST_F(PickerListItemContainerViewTest, EmptyContainerHasNoTopItem) {
  PickerListItemContainerView container;

  EXPECT_EQ(container.GetTopItem(), nullptr);
}

TEST_F(PickerListItemContainerViewTest, GetsBottomItem) {
  PickerListItemContainerView container;

  container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerListItemView* bottom_item = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetBottomItem(), bottom_item);
}

TEST_F(PickerListItemContainerViewTest, EmptyContainerHasNoBottomItem) {
  PickerListItemContainerView container;

  EXPECT_EQ(container.GetBottomItem(), nullptr);
}

TEST_F(PickerListItemContainerViewTest, GetsItemAbove) {
  PickerListItemContainerView container;

  PickerListItemView* item1 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerListItemView* item2 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemAbove(item1), nullptr);
  EXPECT_EQ(container.GetItemAbove(item2), item1);
}

TEST_F(PickerListItemContainerViewTest, ItemNotInContainerHasNoItemAbove) {
  PickerListItemContainerView container;

  PickerListItemView item_not_in_container(base::DoNothing());

  EXPECT_EQ(container.GetItemAbove(&item_not_in_container), nullptr);
}

TEST_F(PickerListItemContainerViewTest, GetsItemBelow) {
  PickerListItemContainerView container;

  PickerListItemView* item1 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerListItemView* item2 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemBelow(item1), item2);
  EXPECT_EQ(container.GetItemBelow(item2), nullptr);
}

TEST_F(PickerListItemContainerViewTest, ItemNotInContainerHasNoItemBelow) {
  PickerListItemContainerView container;

  PickerListItemView item_not_in_container(base::DoNothing());

  EXPECT_EQ(container.GetItemBelow(&item_not_in_container), nullptr);
}

TEST_F(PickerListItemContainerViewTest, NoItemLeftOf) {
  PickerListItemContainerView container;

  PickerListItemView* item1 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerListItemView* item2 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(container.GetItemLeftOf(item2), nullptr);
}

TEST_F(PickerListItemContainerViewTest, NoItemRightOf) {
  PickerListItemContainerView container;

  PickerListItemView* item1 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));
  PickerListItemView* item2 = container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.GetItemRightOf(item1), nullptr);
  EXPECT_EQ(container.GetItemRightOf(item2), nullptr);
}

TEST_F(PickerListItemContainerViewTest, ChildrenHasListItemRole) {
  PickerListItemContainerView container;

  container.AddListItem(
      std::make_unique<PickerListItemView>(base::DoNothing()));

  EXPECT_EQ(container.children()[0]->GetAccessibleRole(),
            ax::mojom::Role::kListItem);
}

}  // namespace
}  // namespace ash
