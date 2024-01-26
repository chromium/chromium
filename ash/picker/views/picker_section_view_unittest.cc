// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <utility>

#include "ash/picker/views/picker_item_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

std::unique_ptr<PickerItemView> CreateSizedItem(
    PickerItemView::ItemType item_type,
    const gfx::Size& size) {
  auto item = std::make_unique<PickerItemView>(views::Button::PressedCallback(),
                                               item_type);
  item->SetPreferredSize(size);
  return item;
}

using PickerSectionViewTest = AshTestBase;

TEST_F(PickerSectionViewTest, OneSmallGridItem) {
  PickerSectionView section_view(u"Section");

  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kSmallGridItem,
                                       gfx::Size(100, 40)));

  // One row with one item.
  EXPECT_THAT(
      section_view.small_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, SmallGridItemsStayWithinMaximumWidth) {
  PickerSectionView section_view(u"Section");

  section_view.SetMaximumWidth(320);
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kSmallGridItem,
                                       gfx::Size(100, 40)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kSmallGridItem,
                                       gfx::Size(80, 40)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kSmallGridItem,
                                       gfx::Size(90, 40)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kSmallGridItem,
                                       gfx::Size(100, 40)));

  // Three items in first row, one item in second row.
  EXPECT_THAT(
      section_view.small_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(3))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, OneLargeGridItem) {
  PickerSectionView section_view(u"Section");

  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 100)));

  // Two columns, one item in the first column.
  EXPECT_THAT(
      section_view.large_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
}

TEST_F(PickerSectionViewTest, TwoLargeGridItems) {
  PickerSectionView section_view(u"Section");

  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 100)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 100)));

  // Two columns, one item in each column.
  EXPECT_THAT(
      section_view.large_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(1)))));
}

TEST_F(PickerSectionViewTest, LargeGridItemsWithVaryingHeight) {
  PickerSectionView section_view(u"Section");

  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 120)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 20)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 30)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 20)));

  // One item in first column, three items in second column.
  EXPECT_THAT(
      section_view.large_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(3)))));
}

TEST_F(PickerSectionViewTest, SmallAndLargeGridItems) {
  PickerSectionView section_view(u"Section");

  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kSmallGridItem,
                                       gfx::Size(100, 40)));
  section_view.AddItem(CreateSizedItem(PickerItemView::ItemType::kLargeGridItem,
                                       gfx::Size(100, 100)));

  // One row with one small grid item.
  EXPECT_THAT(
      section_view.small_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1)))));
  // Two columns, one large grid item in the first column.
  EXPECT_THAT(
      section_view.large_grid_items_container_for_testing()->children(),
      ElementsAre(Pointee(Property(&views::View::children, SizeIs(1))),
                  Pointee(Property(&views::View::children, SizeIs(0)))));
}

}  // namespace
}  // namespace ash
