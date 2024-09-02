// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_row_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/picker/views/picker_image_item_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

std::unique_ptr<PickerImageItemView> CreateImageItem() {
  return std::make_unique<PickerImageItemView>(
      base::DoNothing(),
      std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(
          gfx::test::CreateImageSkia(/*size=*/100))));
}

using PickerImageItemRowViewTest = views::ViewsTestBase;

TEST_F(PickerImageItemRowViewTest, CreatesImageItems) {
  PickerImageItemRowView item_row;

  views::View* item1 = item_row.AddImageItem(CreateImageItem());
  views::View* item2 = item_row.AddImageItem(CreateImageItem());

  // Two columns, one item in each column.
  EXPECT_THAT(item_row.GetItemsContainerForTesting().children(),
              ElementsAre(item1, item2));
}

TEST_F(PickerImageItemRowViewTest, ImageItemsAreResizedToSameWidth) {
  PickerImageItemRowView item_row;
  item_row.SetPreferredSize(gfx::Size(320, 60));

  views::View* item1 = item_row.AddImageItem(CreateImageItem());
  views::View* item2 = item_row.AddImageItem(CreateImageItem());

  EXPECT_EQ(item1->GetPreferredSize().width(),
            item2->GetPreferredSize().width());
}

TEST_F(PickerImageItemRowViewTest, GetsTopItem) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetTopItem(), item1);
}

TEST_F(PickerImageItemRowViewTest, EmptyRowHasNoTopItem) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  EXPECT_EQ(item_row->GetTopItem(), nullptr);
}

TEST_F(PickerImageItemRowViewTest, GetsBottomItem) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetBottomItem(), item1);
}

TEST_F(PickerImageItemRowViewTest, EmptyRowHasNoBottomItem) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  EXPECT_EQ(item_row->GetBottomItem(), nullptr);
}

TEST_F(PickerImageItemRowViewTest, GetsItemAbove) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  views::View* item2 = item_row->AddImageItem(CreateImageItem());
  views::View* item3 = item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetItemAbove(item1), nullptr);
  EXPECT_EQ(item_row->GetItemAbove(item2), nullptr);
  EXPECT_EQ(item_row->GetItemAbove(item3), nullptr);
}

TEST_F(PickerImageItemRowViewTest, ItemNotInRowHasNoItemAbove) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemAbove(item_not_in_row.get()), nullptr);
}

TEST_F(PickerImageItemRowViewTest, GetsItemBelow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  views::View* item2 = item_row->AddImageItem(CreateImageItem());
  views::View* item3 = item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetItemBelow(item1), nullptr);
  EXPECT_EQ(item_row->GetItemBelow(item2), nullptr);
  EXPECT_EQ(item_row->GetItemBelow(item3), nullptr);
}

TEST_F(PickerImageItemRowViewTest, ItemNotInRowHasNoItemBelow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemBelow(item_not_in_row.get()), nullptr);
}

TEST_F(PickerImageItemRowViewTest, GetsItemLeftOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  views::View* item2 = item_row->AddImageItem(CreateImageItem());
  views::View* item3 = item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(item_row->GetItemLeftOf(item2), item1);
  EXPECT_EQ(item_row->GetItemLeftOf(item3), item2);
}

TEST_F(PickerImageItemRowViewTest, ItemNotInRowHasNoItemLeftOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemLeftOf(item_not_in_row.get()), nullptr);
}

TEST_F(PickerImageItemRowViewTest, GetsItemRightOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  views::View* item2 = item_row->AddImageItem(CreateImageItem());
  views::View* item3 = item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetItemRightOf(item1), item2);
  EXPECT_EQ(item_row->GetItemRightOf(item2), item3);
  EXPECT_EQ(item_row->GetItemRightOf(item3), nullptr);
}

TEST_F(PickerImageItemRowViewTest, ItemNotInRowHasNoItemRightOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemRightOf(item_not_in_row.get()), nullptr);
}

}  // namespace
}  // namespace ash
