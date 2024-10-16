// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_submenu_controller.h"

#include <memory>

#include "ash/quick_insert/quick_insert_test_util.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/i18n/rtl.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using QuickInsertSubmenuControllerTest = AshTestBase;

std::vector<std::unique_ptr<QuickInsertListItemView>> CreateSingleItem(
    base::RepeatingClosure callback) {
  std::vector<std::unique_ptr<QuickInsertListItemView>> items;
  items.push_back(
      std::make_unique<QuickInsertListItemView>(std::move(callback)));
  return items;
}

TEST_F(QuickInsertSubmenuControllerTest, ShowsWidget) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());

  controller.Show(anchor_widget->GetContentsView(), {});

  EXPECT_NE(controller.widget_for_testing(), nullptr);
}

TEST_F(QuickInsertSubmenuControllerTest, ShowsWidgetAlignedWithAnchorLTR) {
  base::i18n::SetRTLForTesting(false);
  UpdateDisplay("2000x1000");
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  const gfx::Rect anchor_bounds(345, 123, 100, 100);
  anchor_widget->SetBounds(anchor_bounds);

  controller.Show(anchor_widget->GetContentsView(), {});

  ASSERT_NE(controller.widget_for_testing(), nullptr);
  const gfx::Rect submenu_bounds =
      controller.widget_for_testing()->GetClientAreaBoundsInScreen();
  // The submenu widget should be on the right of the anchor, with a horizontal
  // small overlap.
  EXPECT_NEAR(submenu_bounds.x(), anchor_bounds.right(), 20);
  // The submenu widget should be below the anchor, with a vertical small
  // overlap.
  EXPECT_NEAR(submenu_bounds.y(), anchor_bounds.y(), 20);
}

TEST_F(QuickInsertSubmenuControllerTest, ShowsWidgetAlignedWithAnchorRTL) {
  base::i18n::SetRTLForTesting(true);
  UpdateDisplay("2000x1000");
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  const gfx::Rect anchor_bounds(345, 123, 100, 100);
  anchor_widget->SetBounds(anchor_bounds);

  controller.Show(anchor_widget->GetContentsView(), {});

  ASSERT_NE(controller.widget_for_testing(), nullptr);
  const gfx::Rect submenu_bounds =
      controller.widget_for_testing()->GetClientAreaBoundsInScreen();
  // The submenu widget should be on the left of the anchor, with a horizontal
  // small overlap.
  EXPECT_NEAR(submenu_bounds.right(), anchor_bounds.x(), 20);
  // The submenu widget should be below the anchor, with a vertical small
  // overlap.
  EXPECT_NEAR(submenu_bounds.y(), anchor_bounds.y(), 20);
}

TEST_F(QuickInsertSubmenuControllerTest, ShowsWidgetWithParent) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  controller.Show(anchor_widget->GetContentsView(), {});

  EXPECT_EQ(controller.widget_for_testing()->parent(), anchor_widget.get());
  EXPECT_EQ(controller.widget_for_testing()->GetNativeWindow()->parent(),
            anchor_widget->GetNativeWindow());
}

TEST_F(QuickInsertSubmenuControllerTest, ClosesWidget) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());

  controller.Show(anchor_widget->GetContentsView(), {});
  controller.Close();

  views::test::WidgetDestroyedWaiter(controller.widget_for_testing()).Wait();
}

TEST_F(QuickInsertSubmenuControllerTest,
       ClosesWidgetWhenAnchorWidgetIsDestroyed) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  anchor_widget->Show();
  controller.Show(anchor_widget->GetContentsView(), {});

  anchor_widget->CloseNow();

  EXPECT_EQ(controller.widget_for_testing(), nullptr);
}

TEST_F(QuickInsertSubmenuControllerTest,
       ClosesWidgetWhenAnchorViewIsDestroyed) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  auto* contents_view =
      anchor_widget->SetContentsView(std::make_unique<views::View>());
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  anchor_widget->Show();
  controller.Show(anchor_view, {});

  contents_view->RemoveAllChildViews();

  views::test::WidgetDestroyedWaiter(controller.widget_for_testing()).Wait();
}

TEST_F(QuickInsertSubmenuControllerTest, ClosesWidgetWhenAnchorViewIsHidden) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  auto* contents_view =
      anchor_widget->SetContentsView(std::make_unique<views::View>());
  auto* anchor_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  anchor_widget->Show();
  controller.Show(anchor_view, {});

  contents_view->SetVisible(false);

  views::test::WidgetDestroyedWaiter(controller.widget_for_testing()).Wait();
}

TEST_F(QuickInsertSubmenuControllerTest, GetsSubmenuView) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());

  controller.Show(anchor_widget->GetContentsView(), {});

  EXPECT_NE(controller.GetSubmenuView(), nullptr);
}

TEST_F(QuickInsertSubmenuControllerTest, GetsAnchorView) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  auto* anchor_view =
      anchor_widget->SetContentsView(std::make_unique<views::View>());

  controller.Show(anchor_view, {});

  EXPECT_EQ(controller.GetAnchorView(), anchor_view);
}

TEST_F(QuickInsertSubmenuControllerTest, TriggersCallbackWhenClickingOnItem) {
  PickerSubmenuController controller;
  auto anchor_widget = CreateFramelessTestWidget();
  anchor_widget->SetContentsView(std::make_unique<views::View>());
  base::test::TestFuture<void> select_item_future;
  std::vector<std::unique_ptr<QuickInsertListItemView>> items =
      CreateSingleItem(select_item_future.GetRepeatingCallback());
  auto* top_item_ptr = items.front().get();
  controller.Show(anchor_widget->GetContentsView(), std::move(items));

  ViewDrawnWaiter waiter;
  waiter.Wait(top_item_ptr);
  LeftClickOn(top_item_ptr);

  EXPECT_TRUE(select_item_future.Wait());
}

}  // namespace
}  // namespace ash
