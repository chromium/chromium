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

constexpr int kDefaultGridWidth = 320;

std::unique_ptr<PickerEmoticonItemView> CreateSizedEmoticonItem(
    const gfx::Size& size) {
  auto emoticon_item = std::make_unique<PickerEmoticonItemView>(
      base::DoNothing(), u"¯\\_(ツ)_/¯");
  emoticon_item->SetPreferredSize(size);
  return emoticon_item;
}

using PickerSmallItemGridViewTest = AshTestBase;

TEST_F(PickerSmallItemGridViewTest, AddsEmojiItem) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  small_item_grid.AddEmojiItem(
      std::make_unique<PickerEmojiItemView>(base::DoNothing(), u"😊"));

  // One row with one item.
  EXPECT_THAT(
      small_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSmallItemGridViewTest, AddsSymbolItem) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  small_item_grid.AddSymbolItem(
      std::make_unique<PickerSymbolItemView>(base::DoNothing(), u"♬"));

  // One row with one item.
  EXPECT_THAT(
      small_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSmallItemGridViewTest, AddsEmoticonItem) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  small_item_grid.AddEmoticonItem(std::make_unique<PickerEmoticonItemView>(
      base::DoNothing(), u"¯\\_(ツ)_/¯"));

  // One row with one item.
  EXPECT_THAT(
      small_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSmallItemGridViewTest, SmallGridItemsStayWithinGridWidth) {
  PickerSmallItemGridView small_item_grid(kDefaultGridWidth);

  small_item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));
  small_item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(80, 40)));
  small_item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(90, 40)));
  small_item_grid.AddEmoticonItem(CreateSizedEmoticonItem(gfx::Size(100, 40)));

  // Three items in first row, one item in second row.
  EXPECT_THAT(
      small_item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(3))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
}

}  // namespace
}  // namespace ash
