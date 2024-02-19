// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <utility>

#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/test/ash_test_base.h"
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

constexpr int kDefaultSectionWidth = 320;

int GetAspectRatio(const gfx::Size& size) {
  return size.height() / size.width();
}

std::unique_ptr<PickerEmoticonItemView> CreateSizedEmoticonItem(
    const gfx::Size& size) {
  auto emoticon_item = std::make_unique<PickerEmoticonItemView>(
      base::DoNothing(), u"¯\\_(ツ)_/¯");
  emoticon_item->SetPreferredSize(size);
  return emoticon_item;
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

using PickerSectionViewTest = AshTestBase;

TEST_F(PickerSectionViewTest, CreatesTitleLabel) {
  PickerSectionView section_view(kDefaultSectionWidth);

  const std::u16string kSectionTitleText = u"Section";
  section_view.AddTitleLabel(kSectionTitleText);

  EXPECT_THAT(section_view.title_label_for_testing(),
              Property(&views::Label::GetText, Eq(kSectionTitleText)));
}

TEST_F(PickerSectionViewTest, AddsEmojiItem) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));

  // One row with one item.
  EXPECT_THAT(
      section_view.small_items_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, AddsSymbolItem) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));

  // One row with one item.
  EXPECT_THAT(
      section_view.small_items_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, AddsEmoticonItem) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddEmoticonItem(std::make_unique<PickerEmoticonItemView>(
      base::DoNothing(), u"¯\\_(ツ)_/¯"));

  // One row with one item.
  EXPECT_THAT(
      section_view.small_items_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, SmallGridItemsStayWithinSectionWidth) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  section_view.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(80, 40)));
  section_view.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(90, 40)));
  section_view.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  // Three items in first row, one item in second row.
  EXPECT_THAT(
      section_view.small_items_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(3))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, OneGifItem) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in the first column.
  EXPECT_THAT(
      section_view.image_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
}

TEST_F(PickerSectionViewTest, TwoGifItems) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in each column.
  EXPECT_THAT(
      section_view.image_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, GifItemsWithVaryingHeight) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 20)));
  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 30)));
  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 20)));

  // One item in first column, three items in second column.
  EXPECT_THAT(
      section_view.image_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(3)))));
}

TEST_F(PickerSectionViewTest, GifItemsAreResizedToSameWidth) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  section_view.AddImageItem(CreateGifItem(gfx::Size(80, 160)));

  const views::View::Views& columns =
      section_view.image_grid_for_testing()->children();
  ASSERT_THAT(
      columns,
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
  EXPECT_EQ(columns[0]->children()[0]->GetPreferredSize().width(),
            columns[1]->children()[0]->GetPreferredSize().width());
}

TEST_F(PickerSectionViewTest, PreservesAspectRatioOfGifItems) {
  PickerSectionView section_view(kDefaultSectionWidth);

  constexpr gfx::Size kGifDimensions(100, 200);
  section_view.AddImageItem(CreateGifItem(kGifDimensions));

  const views::View::Views& columns =
      section_view.image_grid_for_testing()->children();
  ASSERT_THAT(
      columns,
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
  EXPECT_EQ(GetAspectRatio(columns[0]->children()[0]->GetPreferredSize()),
            GetAspectRatio(kGifDimensions));
}

TEST_F(PickerSectionViewTest, EmojiItemsAndGifItems) {
  PickerSectionView section_view(kDefaultSectionWidth);

  section_view.AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));
  section_view.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // One row with one small grid item.
  EXPECT_THAT(
      section_view.small_items_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
  // Two columns, one gif item in the first column.
  EXPECT_THAT(
      section_view.image_grid_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
}

}  // namespace
}  // namespace ash
