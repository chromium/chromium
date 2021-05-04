// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble.h"

#include <set>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using views::test::WidgetDestroyedWaiter;

namespace ash {
namespace {

// Returns the number of widgets in the app list container on the primary
// display.
size_t NumberOfWidgetsInAppListContainer() {
  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::set<views::Widget*> widgets;
  views::Widget::GetAllChildWidgets(container, &widgets);
  return widgets.size();
}

using AppListBubbleTest = AshTestBase;

TEST_F(AppListBubbleTest, ShowOpensOneWidgetInAppListContainer) {
  AppListBubble bubble;
  bubble.Show(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, DismissClosesWidget) {
  AppListBubble bubble;
  bubble.Show(GetPrimaryDisplay().id());

  WidgetDestroyedWaiter waiter(bubble.bubble_widget_for_test());
  bubble.Dismiss();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, ToggleOpensOneWidgetInAppListContainer) {
  AppListBubble bubble;
  bubble.Toggle(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, ToggleClosesWidgetInAppListContainer) {
  AppListBubble bubble;
  bubble.Toggle(GetPrimaryDisplay().id());

  WidgetDestroyedWaiter waiter(bubble.bubble_widget_for_test());
  bubble.Toggle(GetPrimaryDisplay().id());
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, BubbleIsNotShowingByDefault) {
  AppListBubble bubble;

  EXPECT_FALSE(bubble.IsShowing());
}

TEST_F(AppListBubbleTest, BubbleIsShowingAfterShow) {
  AppListBubble bubble;
  bubble.Show(GetPrimaryDisplay().id());

  EXPECT_TRUE(bubble.IsShowing());
}

TEST_F(AppListBubbleTest, BubbleIsNotShowingAfterDismiss) {
  AppListBubble bubble;
  bubble.Show(GetPrimaryDisplay().id());
  bubble.Dismiss();

  EXPECT_FALSE(bubble.IsShowing());
}

TEST_F(AppListBubbleTest, DoesNotCrashWhenNativeWidgetDestroyed) {
  AppListBubble bubble;
  bubble.Show(GetPrimaryDisplay().id());

  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  ASSERT_EQ(1u, container->children().size());
  aura::Window* native_window = container->children()[0];
  delete native_window;

  // No crash.
}

}  // namespace
}  // namespace ash
