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

using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

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
  PickerImageItemGridView image_item_grid(kDefaultGridWidth);

  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in the first column.
  EXPECT_THAT(
      image_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
}

TEST(PickerImageItemGridViewTest, TwoGifItems) {
  PickerImageItemGridView image_item_grid(kDefaultGridWidth);

  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in each column.
  EXPECT_THAT(
      image_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST(PickerImageItemGridViewTest, GifItemsWithVaryingHeight) {
  PickerImageItemGridView image_item_grid(kDefaultGridWidth);

  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 20)));
  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 30)));
  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 20)));

  // One item in first column, three items in second column.
  EXPECT_THAT(
      image_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(3)))));
}

TEST(PickerImageItemGridViewTest, GifItemsAreResizedToSameWidth) {
  PickerImageItemGridView image_item_grid(kDefaultGridWidth);

  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  image_item_grid.AddImageItem(CreateGifItem(gfx::Size(80, 160)));

  const views::View::Views& columns = image_item_grid.children();
  ASSERT_THAT(
      columns,
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
  EXPECT_EQ(columns[0]->children()[0]->GetPreferredSize().width(),
            columns[1]->children()[0]->GetPreferredSize().width());
}

TEST(PickerImageItemGridViewTest, PreservesAspectRatioOfGifItems) {
  PickerImageItemGridView image_item_grid(kDefaultGridWidth);

  constexpr gfx::Size kGifDimensions(100, 200);
  image_item_grid.AddImageItem(CreateGifItem(kGifDimensions));

  const views::View::Views& columns = image_item_grid.children();
  ASSERT_THAT(
      columns,
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
  EXPECT_EQ(GetAspectRatio(columns[0]->children()[0]->GetPreferredSize()),
            GetAspectRatio(kGifDimensions));
}

}  // namespace
}  // namespace ash
