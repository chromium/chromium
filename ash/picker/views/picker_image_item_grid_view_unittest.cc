// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_grid_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Property;

constexpr int kDefaultGridWidth = 320;

int GetAspectRatio(const gfx::Size& size) {
  return size.height() / size.width();
}

std::unique_ptr<PickerImageItemView> CreateGifItem(
    const gfx::Size& gif_dimensions) {
  return std::make_unique<PickerImageItemView>(
      base::DoNothing(),
      std::make_unique<PickerGifView>(
          /*frames_fetcher=*/base::DoNothing(),
          /*preview_image_fetcher=*/base::DoNothing(), gif_dimensions,
          /*accessible_name=*/u""));
}

TEST(PickerImageItemGridViewTest, OneGifItem) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  const PickerItemView* item =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in the first column.
  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, ElementsAre(item))),
                  Pointee(Property(&views::View::children, IsEmpty()))));
}

TEST(PickerImageItemGridViewTest, TwoGifItems) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  const PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  const PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in each column.
  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1))),
          Pointee(Property(&views::View::children, ElementsAre(item2)))));
}

TEST(PickerImageItemGridViewTest, GifItemsWithVaryingHeight) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  const PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  const PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 20)));
  const PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 30)));
  const PickerItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 20)));

  // One item in first column, three items in second column.
  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, ElementsAre(item1))),
                  Pointee(Property(&views::View::children,
                                   ElementsAre(item2, item3, item4)))));
}

TEST(PickerImageItemGridViewTest, GifItemsAreResizedToSameWidth) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  const PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  const PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(80, 160)));

  EXPECT_EQ(item1->GetPreferredSize().width(),
            item2->GetPreferredSize().width());
}

TEST(PickerImageItemGridViewTest, PreservesAspectRatioOfGifItems) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  constexpr gfx::Size kGifDimensions(100, 200);
  const PickerItemView* item =
      item_grid.AddImageItem(CreateGifItem(kGifDimensions));

  EXPECT_EQ(GetAspectRatio(item->GetPreferredSize()),
            GetAspectRatio(kGifDimensions));
}

TEST(PickerImageItemGridViewTest, GetsTopItem) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item3))),
          Pointee(Property(&views::View::children, ElementsAre(item2)))));
  EXPECT_EQ(item_grid.GetTopItem(), item1);
}

TEST(PickerImageItemGridViewTest, EmptyGridHasNoTopItem) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  EXPECT_EQ(item_grid.GetTopItem(), nullptr);
}

TEST(PickerImageItemGridViewTest, GetsBottomItem) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children, ElementsAre(item1, item3))),
          Pointee(Property(&views::View::children, ElementsAre(item2)))));
  EXPECT_EQ(item_grid.GetBottomItem(), item3);
}

TEST(PickerImageItemGridViewTest, EmptyGridHasNoBottomItem) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  EXPECT_EQ(item_grid.GetBottomItem(), nullptr);
}

TEST(PickerImageItemGridViewTest, GetsItemAbove) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  PickerItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1, item3))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2, item4)))));
  EXPECT_EQ(item_grid.GetItemAbove(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemAbove(item2), nullptr);
  EXPECT_EQ(item_grid.GetItemAbove(item3), item1);
  EXPECT_EQ(item_grid.GetItemAbove(item4), item2);
}

TEST(PickerImageItemGridViewTest, ItemNotInGridHasNoItemAbove) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<PickerImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemAbove(item_not_in_grid.get()), nullptr);
}

TEST(PickerImageItemGridViewTest, GetsItemBelow) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  PickerItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1, item3))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2, item4)))));
  EXPECT_EQ(item_grid.GetItemBelow(item1), item3);
  EXPECT_EQ(item_grid.GetItemBelow(item2), item4);
  EXPECT_EQ(item_grid.GetItemBelow(item3), nullptr);
  EXPECT_EQ(item_grid.GetItemBelow(item4), nullptr);
}

TEST(PickerImageItemGridViewTest, ItemNotInGridHasNoItemBelow) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<PickerImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemBelow(item_not_in_grid.get()), nullptr);
}

TEST(PickerImageItemGridViewTest, GetsItemLeftOf) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  PickerItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1, item3))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2, item4)))));
  EXPECT_EQ(item_grid.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item2), item1);
  EXPECT_EQ(item_grid.GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item4), item3);
}

TEST(PickerImageItemGridViewTest, GetsItemLeftOfWithUnbalancedColumns) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 300)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, ElementsAre(item1))),
                  Pointee(Property(&views::View::children,
                                   ElementsAre(item2, item3)))));
  EXPECT_EQ(item_grid.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item2), item1);
  EXPECT_EQ(item_grid.GetItemLeftOf(item3), item1);
}

TEST(PickerImageItemGridViewTest, ItemNotInGridHasNoItemLeftOf) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<PickerImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemLeftOf(item_not_in_grid.get()), nullptr);
}

TEST(PickerImageItemGridViewTest, GetsItemRightOf) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);

  PickerItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  PickerItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  PickerItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  PickerItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1, item3))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2, item4)))));
  EXPECT_EQ(item_grid.GetItemRightOf(item1), item2);
  EXPECT_EQ(item_grid.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(item_grid.GetItemRightOf(item3), item4);
  EXPECT_EQ(item_grid.GetItemRightOf(item4), nullptr);
}

TEST(PickerImageItemGridViewTest, ItemNotInGridHasNoItemRightOf) {
  PickerImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<PickerImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemRightOf(item_not_in_grid.get()), nullptr);
}

}  // namespace
}  // namespace ash
