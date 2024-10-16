// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_image_item_row_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Pointee;
using ::testing::Property;

std::unique_ptr<PickerImageItemView> CreateImageItem() {
  return std::make_unique<PickerImageItemView>(
      std::make_unique<views::ImageView>(ui::ImageModel::FromImageSkia(
          gfx::test::CreateImageSkia(/*size=*/100))),
      u"image", base::DoNothing());
}

using QuickInsertImageItemRowViewTest = views::ViewsTestBase;

TEST_F(QuickInsertImageItemRowViewTest, HasGridRole) {
  PickerImageItemRowView item_row;

  EXPECT_EQ(item_row.GetAccessibleRole(), ax::mojom::Role::kGrid);
}

TEST_F(QuickInsertImageItemRowViewTest, HasRowOfItems) {
  PickerImageItemRowView item_row;

  EXPECT_THAT(item_row.children(),
              Contains(Pointee(Property(&views::View::GetAccessibleRole,
                                        ax::mojom::Role::kRow))));
}

TEST_F(QuickInsertImageItemRowViewTest, CreatesImageItems) {
  PickerImageItemRowView item_row;

  views::View* item1 = item_row.AddImageItem(CreateImageItem());
  views::View* item2 = item_row.AddImageItem(CreateImageItem());

  // Two columns, one item in each column.
  EXPECT_THAT(item_row.GetItems(), ElementsAre(item1, item2));
}

TEST_F(QuickInsertImageItemRowViewTest, ImageItemsAreResizedToSameWidth) {
  PickerImageItemRowView item_row;
  item_row.SetPreferredSize(gfx::Size(320, 60));

  views::View* item1 = item_row.AddImageItem(CreateImageItem());
  views::View* item2 = item_row.AddImageItem(CreateImageItem());

  EXPECT_EQ(item1->GetPreferredSize().width(),
            item2->GetPreferredSize().width());
}

TEST_F(QuickInsertImageItemRowViewTest, GetsTopItem) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetTopItem(), item1);
}

TEST_F(QuickInsertImageItemRowViewTest, EmptyRowTopItemIsMoreItemsButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  EXPECT_EQ(item_row->GetTopItem(), item_row->GetMoreItemsButtonForTesting());
}

TEST_F(QuickInsertImageItemRowViewTest, GetsBottomItem) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());
  item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetBottomItem(), item1);
}

TEST_F(QuickInsertImageItemRowViewTest, EmptyRowBottomItemIsMoreItemsButton) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  EXPECT_EQ(item_row->GetBottomItem(),
            item_row->GetMoreItemsButtonForTesting());
}

TEST_F(QuickInsertImageItemRowViewTest, GetsItemAbove) {
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
  EXPECT_EQ(item_row->GetItemAbove(item_row->GetMoreItemsButtonForTesting()),
            nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, ItemNotInRowHasNoItemAbove) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemAbove(item_not_in_row.get()), nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, GetsItemBelow) {
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
  EXPECT_EQ(item_row->GetItemBelow(item_row->GetMoreItemsButtonForTesting()),
            nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, ItemNotInRowHasNoItemBelow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemBelow(item_not_in_row.get()), nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, GetsItemLeftOf) {
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
  EXPECT_EQ(item_row->GetItemLeftOf(item_row->GetMoreItemsButtonForTesting()),
            item3);
}

TEST_F(QuickInsertImageItemRowViewTest, ItemLeftOfMoreItemsButtonInEmptyRow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  EXPECT_EQ(item_row->GetItemLeftOf(item_row->GetMoreItemsButtonForTesting()),
            nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, ItemNotInRowHasNoItemLeftOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemLeftOf(item_not_in_row.get()), nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, GetsItemRightOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  views::View* item1 = item_row->AddImageItem(CreateImageItem());
  views::View* item2 = item_row->AddImageItem(CreateImageItem());
  views::View* item3 = item_row->AddImageItem(CreateImageItem());

  EXPECT_EQ(item_row->GetItemRightOf(item1), item2);
  EXPECT_EQ(item_row->GetItemRightOf(item2), item3);
  EXPECT_EQ(item_row->GetItemRightOf(item3),
            item_row->GetMoreItemsButtonForTesting());
  EXPECT_EQ(item_row->GetItemRightOf(item_row->GetMoreItemsButtonForTesting()),
            nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, ItemRightOfMoreItemsButtonInEmptyRow) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());

  EXPECT_EQ(item_row->GetItemRightOf(item_row->GetMoreItemsButtonForTesting()),
            nullptr);
}

TEST_F(QuickInsertImageItemRowViewTest, ItemNotInRowHasNoItemRightOf) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  PickerImageItemRowView* item_row =
      widget->SetContentsView(std::make_unique<PickerImageItemRowView>());
  std::unique_ptr<PickerImageItemView> item_not_in_row = CreateImageItem();

  EXPECT_EQ(item_row->GetItemRightOf(item_not_in_row.get()), nullptr);
}

}  // namespace
}  // namespace ash
