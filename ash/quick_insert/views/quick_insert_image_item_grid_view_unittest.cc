// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_image_item_grid_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/quick_insert/views/quick_insert_gif_view.h"
#include "ash/quick_insert/views/quick_insert_image_item_view.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/views_test_base.h"
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

std::unique_ptr<QuickInsertImageItemView> CreateGifItem(
    const gfx::Size& gif_dimensions) {
  return std::make_unique<QuickInsertImageItemView>(
      std::make_unique<QuickInsertGifView>(
          /*frames_fetcher=*/base::IgnoreArgs<
              QuickInsertGifView::FramesFetchedCallback>(
              base::ReturnValueOnce<std::unique_ptr<network::SimpleURLLoader>>(
                  nullptr)),
          /*preview_image_fetcher=*/
          base::IgnoreArgs<QuickInsertGifView::PreviewImageFetchedCallback>(
              base::ReturnValueOnce<std::unique_ptr<network::SimpleURLLoader>>(
                  nullptr)),
          gif_dimensions),
      u"gif", base::DoNothing());
}

using QuickInsertImageItemGridViewTest = views::ViewsTestBase;

TEST_F(QuickInsertImageItemGridViewTest, OneGifItem) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  const QuickInsertItemView* item =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in the first column.
  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children,
                                   ElementsAre(item->parent()))),
                  Pointee(Property(&views::View::children, IsEmpty()))));
}

TEST_F(QuickInsertImageItemGridViewTest, TwoGifItems) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  const QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  const QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));

  // Two columns, one item in each column.
  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1->parent()))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2->parent())))));
}

TEST_F(QuickInsertImageItemGridViewTest, GifItemsWithVaryingHeight) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  const QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  const QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 20)));
  const QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 30)));
  const QuickInsertItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 20)));

  // One item in first column, three items in second column.
  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(Pointee(Property(&views::View::children,
                                   ElementsAre(item1->parent()))),
                  Pointee(Property(&views::View::children,
                                   ElementsAre(item2->parent(), item3->parent(),
                                               item4->parent())))));
}

TEST_F(QuickInsertImageItemGridViewTest, GifItemsAreResizedToSameWidth) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetSize(gfx::Size(90, 100));
  QuickInsertImageItemGridView* item_grid = widget->SetContentsView(
      std::make_unique<QuickInsertImageItemGridView>(90));
  QuickInsertItemView* item1 =
      item_grid->AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid->AddImageItem(CreateGifItem(gfx::Size(80, 160)));
  widget->Show();
  ViewDrawnWaiter().Wait(item1);

  ASSERT_GT(item1->width(), 0);
  EXPECT_EQ(item1->width(), item2->width());
}

TEST_F(QuickInsertImageItemGridViewTest, PreservesAspectRatioOfGifItems) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  QuickInsertImageItemGridView* item_grid = widget->SetContentsView(
      std::make_unique<QuickInsertImageItemGridView>(kDefaultGridWidth));
  constexpr gfx::Size kGifDimensions(100, 200);
  QuickInsertItemView* item =
      item_grid->AddImageItem(CreateGifItem(kGifDimensions));
  widget->Show();
  ViewDrawnWaiter().Wait(item);

  ASSERT_GT(item->width(), 0);
  EXPECT_EQ(GetAspectRatio(item->GetLocalBounds().size()),
            GetAspectRatio(kGifDimensions));
}

TEST_F(QuickInsertImageItemGridViewTest, GetsTopItem) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(
                              &views::View::children,
                              ElementsAre(item1->parent(), item3->parent()))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2->parent())))));
  EXPECT_EQ(item_grid.GetTopItem(), item1);
}

TEST_F(QuickInsertImageItemGridViewTest, EmptyGridHasNoTopItem) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  EXPECT_EQ(item_grid.GetTopItem(), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, GetsBottomItem) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(
                              &views::View::children,
                              ElementsAre(item1->parent(), item3->parent()))),
                          Pointee(Property(&views::View::children,
                                           ElementsAre(item2->parent())))));
  EXPECT_EQ(item_grid.GetBottomItem(), item3);
}

TEST_F(QuickInsertImageItemGridViewTest, EmptyGridHasNoBottomItem) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  EXPECT_EQ(item_grid.GetBottomItem(), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, GetsItemAbove) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  QuickInsertItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children,
                           ElementsAre(item1->parent(), item3->parent()))),
          Pointee(Property(&views::View::children,
                           ElementsAre(item2->parent(), item4->parent())))));
  EXPECT_EQ(item_grid.GetItemAbove(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemAbove(item2), nullptr);
  EXPECT_EQ(item_grid.GetItemAbove(item3), item1);
  EXPECT_EQ(item_grid.GetItemAbove(item4), item2);
}

TEST_F(QuickInsertImageItemGridViewTest, ItemNotInGridHasNoItemAbove) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<QuickInsertImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemAbove(item_not_in_grid.get()), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, GetsItemBelow) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  QuickInsertItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children,
                           ElementsAre(item1->parent(), item3->parent()))),
          Pointee(Property(&views::View::children,
                           ElementsAre(item2->parent(), item4->parent())))));
  EXPECT_EQ(item_grid.GetItemBelow(item1), item3);
  EXPECT_EQ(item_grid.GetItemBelow(item2), item4);
  EXPECT_EQ(item_grid.GetItemBelow(item3), nullptr);
  EXPECT_EQ(item_grid.GetItemBelow(item4), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, ItemNotInGridHasNoItemBelow) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<QuickInsertImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemBelow(item_not_in_grid.get()), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, GetsItemLeftOf) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  QuickInsertItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children,
                           ElementsAre(item1->parent(), item3->parent()))),
          Pointee(Property(&views::View::children,
                           ElementsAre(item2->parent(), item4->parent())))));
  EXPECT_EQ(item_grid.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item2), item1);
  EXPECT_EQ(item_grid.GetItemLeftOf(item3), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item4), item3);
}

TEST_F(QuickInsertImageItemGridViewTest, GetsItemLeftOfWithUnbalancedColumns) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 300)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));

  EXPECT_THAT(item_grid.children(),
              ElementsAre(Pointee(Property(&views::View::children,
                                           ElementsAre(item1->parent()))),
                          Pointee(Property(
                              &views::View::children,
                              ElementsAre(item2->parent(), item3->parent())))));
  EXPECT_EQ(item_grid.GetItemLeftOf(item1), nullptr);
  EXPECT_EQ(item_grid.GetItemLeftOf(item2), item1);
  EXPECT_EQ(item_grid.GetItemLeftOf(item3), item1);
}

TEST_F(QuickInsertImageItemGridViewTest, ItemNotInGridHasNoItemLeftOf) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<QuickInsertImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemLeftOf(item_not_in_grid.get()), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, GetsItemRightOf) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);

  QuickInsertItemView* item1 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 110)));
  QuickInsertItemView* item3 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 120)));
  QuickInsertItemView* item4 =
      item_grid.AddImageItem(CreateGifItem(gfx::Size(100, 130)));

  EXPECT_THAT(
      item_grid.children(),
      ElementsAre(
          Pointee(Property(&views::View::children,
                           ElementsAre(item1->parent(), item3->parent()))),
          Pointee(Property(&views::View::children,
                           ElementsAre(item2->parent(), item4->parent())))));
  EXPECT_EQ(item_grid.GetItemRightOf(item1), item2);
  EXPECT_EQ(item_grid.GetItemRightOf(item2), nullptr);
  EXPECT_EQ(item_grid.GetItemRightOf(item3), item4);
  EXPECT_EQ(item_grid.GetItemRightOf(item4), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, ItemNotInGridHasNoItemRightOf) {
  QuickInsertImageItemGridView item_grid(kDefaultGridWidth);
  std::unique_ptr<QuickInsertImageItemView> item_not_in_grid =
      CreateGifItem(gfx::Size(100, 100));

  EXPECT_EQ(item_grid.GetItemRightOf(item_not_in_grid.get()), nullptr);
}

TEST_F(QuickInsertImageItemGridViewTest, TabFocusesFirstItemOnly) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  QuickInsertImageItemGridView* item_grid = widget->SetContentsView(
      std::make_unique<QuickInsertImageItemGridView>(kDefaultGridWidth));

  QuickInsertItemView* item1 =
      item_grid->AddImageItem(CreateGifItem(gfx::Size(100, 100)));
  QuickInsertItemView* item2 =
      item_grid->AddImageItem(CreateGifItem(gfx::Size(100, 110)));

  views::FocusManager* focus_manager = item_grid->GetFocusManager();
  ASSERT_TRUE(focus_manager);
  EXPECT_TRUE(item1->IsFocusable());
  EXPECT_FALSE(item2->IsFocusable());
  EXPECT_EQ(focus_manager->GetNextFocusableView(
                item1, widget.get(), /*reverse=*/false, /*dont_loop=*/true),
            nullptr);
  EXPECT_EQ(focus_manager->GetNextFocusableView(
                item1, widget.get(), /*reverse=*/true, /*dont_loop*/ true),
            nullptr);
}

}  // namespace
}  // namespace ash
