// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_small_item_grid_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::Property;

constexpr int kDefaultGridWidth = 320;

std::unique_ptr<PickerEmoticonItemView> CreateSizedEmoticonItem(
    const gfx::Size& size) {
  auto emoticon_item = std::make_unique<PickerEmoticonItemView>(
      base::DoNothing(), u"¯\\_(ツ)_/¯");
  emoticon_item->SetPreferredSize(size);
  return emoticon_item;
}

using PickerSmallItemGridViewTest = views::ViewsTestBase;

TEST_F(PickerSmallItemGridViewTest, AddsEmojiItem) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  PickerItemView* item = small_item_grid.AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));

  // One row with one item.
  EXPECT_THAT(small_item_grid.children(),
              ElementsAre(Pointee(
                  Property(&views::View::children, ElementsAre(item)))));
}

TEST_F(PickerSmallItemGridViewTest, AddsSymbolItem) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  PickerItemView* item = small_item_grid.AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));

  // One row with one item.
  EXPECT_THAT(small_item_grid.children(),
              ElementsAre(Pointee(
                  Property(&views::View::children, ElementsAre(item)))));
}

TEST_F(PickerSmallItemGridViewTest, AddsEmoticonItem) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  PickerItemView* item =
      small_item_grid.AddEmoticonItem(std::make_unique<PickerEmoticonItemView>(
          base::DoNothing(), u"¯\\_(ツ)_/¯"));

  // One row with one item.
  EXPECT_THAT(small_item_grid.children(),
              ElementsAre(Pointee(
                  Property(&views::View::children, ElementsAre(item)))));
}

TEST_F(PickerSmallItemGridViewTest, SmallGridItemsStayWithinGridWidth) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth,
                                          /*max_visible_rows=*/2);

  PickerItemView* item1 = small_item_grid.AddEmoticonItem(
      CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 = small_item_grid.AddEmoticonItem(
      CreateSizedEmoticonItem(gfx::Size(80, 40)));
  PickerItemView* item3 = small_item_grid.AddEmoticonItem(
      CreateSizedEmoticonItem(gfx::Size(90, 40)));
  PickerItemView* item4 = small_item_grid.AddEmoticonItem(
      CreateSizedEmoticonItem(gfx::Size(100, 40)));

  // Three items in first row, one item in second row.
  EXPECT_THAT(small_item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1, item2, item3))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item4)))));
}

TEST_F(PickerSmallItemGridViewTest, HidesRowsOutsideMaximumVisibleRows) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth,
                                          /*max_visible_rows=*/1);

  small_item_grid.AddEmoticonItem(
      CreateSizedEmoticonItem(gfx::Size(kDefaultGridWidth, 40)));
  small_item_grid.AddEmoticonItem(
      CreateSizedEmoticonItem(gfx::Size(kDefaultGridWidth, 40)));

  // First row visible, second row hidden.
  EXPECT_THAT(small_item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::GetVisible, true)),
                          Pointee(Property(&views::View::GetVisible, false))));
}

TEST_F(PickerSmallItemGridViewTest, GetsTopItem) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth, /*max_visible_rows=*/2);

  PickerItemView* item1 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(200, 40)));
  PickerItemView* item3 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item2))),
          Pointee(Property(&views::View::children, ElementsAre(item3)))));
  EXPECT_EQ(item_grid.GetTopItem(), item1);
}

TEST_F(PickerSmallItemGridViewTest, EmptyGridHasNoTopItem) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth);

  EXPECT_EQ(item_grid.GetTopItem(), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, GetsBottomItem) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth, /*max_visible_rows=*/2);

  PickerItemView* item1 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(200, 40)));
  PickerItemView* item3 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item2))),
          Pointee(Property(&views::View::children, ElementsAre(item3)))));
  EXPECT_EQ(item_grid.GetBottomItem(), item3);
}

TEST_F(PickerSmallItemGridViewTest, EmptyContainerHasNoBottomItem) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth);

  EXPECT_EQ(item_grid.GetBottomItem(), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, GetsItemAbove) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth, /*max_visible_rows=*/2);

  PickerItemView* item1 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(200, 40)));
  PickerItemView* item3 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item2))),
          Pointee(Property(&views::View::children, ElementsAre(item3)))));
  EXPECT_EQ(item_grid.GetItemAbove(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemAbove(item3), item1);
}

TEST_F(PickerSmallItemGridViewTest, ItemNotInContainerHasNoItemAbove) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth);
  PickerEmoticonItemView item_not_in_container(base::DoNothing(),
                                               u"¯\\_(ツ)_/¯");

  EXPECT_EQ(item_grid.GetItemAbove(&item_not_in_container), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, GetsItemBelow) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth, /*max_visible_rows=*/2);

  PickerItemView* item1 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(200, 40)));
  PickerItemView* item3 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item2))),
          Pointee(Property(&views::View::children, ElementsAre(item3)))));
  EXPECT_EQ(item_grid.GetItemBelow(item1), item3);
  EXPECT_EQ(item_grid.GetItemBelow(item2), item3);
  EXPECT_EQ(item_grid.GetItemBelow(item3), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, ItemNotInContainerHasNoItemBelow) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth);
  PickerEmoticonItemView item_not_in_container(base::DoNothing(),
                                               u"¯\\_(ツ)_/¯");

  EXPECT_EQ(item_grid.GetItemBelow(&item_not_in_container), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, GetsItemLeftOf) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth, /*max_visible_rows=*/2);

  PickerItemView* item1 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(200, 40)));
  PickerItemView* item3 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item2))),
          Pointee(Property(&views::View::children, ElementsAre(item3)))));
  EXPECT_EQ(item_grid.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item2), item1);
  EXPECT_EQ(item_grid.GetItemLeftOf(item3), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, ItemNotInContainerHasNoItemLeftOf) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth);
  PickerEmoticonItemView item_not_in_container(base::DoNothing(),
                                               u"¯\\_(ツ)_/¯");

  EXPECT_EQ(item_grid.GetItemLeftOf(&item_not_in_container), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, GetsItemRightOf) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth, /*max_visible_rows=*/2);

  PickerItemView* item1 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  PickerItemView* item2 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(200, 40)));
  PickerItemView* item3 =
      item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item2))),
          Pointee(Property(&views::View::children, ElementsAre(item3)))));
  EXPECT_EQ(item_grid.GetItemRightOf(item1), item2);
  EXPECT_EQ(item_grid.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(item_grid.GetItemRightOf(item3), nullptr);
}

TEST_F(PickerSmallItemGridViewTest, ItemNotInContainerHasNoItemRightOf) {
  PickerSmallItemGridView item_grid(kDefaultGridWidth);
  PickerEmoticonItemView item_not_in_container(base::DoNothing(),
                                               u"¯\\_(ツ)_/¯");

  EXPECT_EQ(item_grid.GetItemRightOf(&item_not_in_container), nullptr);
}

}  // namespace
}  // namespace ash
