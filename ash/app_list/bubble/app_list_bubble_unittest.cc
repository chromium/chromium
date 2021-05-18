// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble.h"

#include <set>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using views::Widget;
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

class AppListBubbleTest : public AshTestBase {
 public:
  AppListBubbleTest() {
    scoped_features_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubbleTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    // Use a realistic screen size so the default size bubble will fit.
    UpdateDisplay("1366x768");
  }

  // Returns the AppListBubble instance. Use this instead of creating a new
  // AppListBubble instance in each test to avoid situations where two bubbles
  // exist at the same time (the per-test one and the "production" one).
  AppListBubble* GetAppListBubble() {
    return Shell::Get()->app_list_controller()->app_list_bubble_for_test();
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AppListBubbleTest, ShowOpensOneWidgetInAppListContainer) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, DismissClosesWidget) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  WidgetDestroyedWaiter waiter(bubble->bubble_widget_for_test());
  bubble->Dismiss();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, DismissWhenNotShowingDoesNotCrash) {
  AppListBubble* bubble = GetAppListBubble();
  EXPECT_FALSE(bubble->IsShowing());

  bubble->Dismiss();
  // No crash.
}

TEST_F(AppListBubbleTest, ToggleOpensOneWidgetInAppListContainer) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Toggle(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, ToggleClosesWidgetInAppListContainer) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Toggle(GetPrimaryDisplay().id());

  WidgetDestroyedWaiter waiter(bubble->bubble_widget_for_test());
  bubble->Toggle(GetPrimaryDisplay().id());
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubbleTest, BubbleIsNotShowingByDefault) {
  AppListBubble* bubble = GetAppListBubble();

  EXPECT_FALSE(bubble->IsShowing());
}

TEST_F(AppListBubbleTest, BubbleIsShowingAfterShow) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  EXPECT_TRUE(bubble->IsShowing());
}

TEST_F(AppListBubbleTest, BubbleIsNotShowingAfterDismiss) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());
  bubble->Dismiss();

  EXPECT_FALSE(bubble->IsShowing());
}

TEST_F(AppListBubbleTest, DoesNotCrashWhenNativeWidgetDestroyed) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  aura::Window* container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  ASSERT_EQ(1u, container->children().size());
  aura::Window* native_window = container->children()[0];
  delete native_window;
  // No crash.

  // Trigger an event that would normally be handled by the event filter.
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();
  // No crash.
}

TEST_F(AppListBubbleTest, ClickInTopLeftOfScreenClosesBubble) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  Widget* widget = bubble->bubble_widget_for_test();
  WidgetDestroyedWaiter waiter(widget);
  ASSERT_FALSE(widget->GetWindowBoundsInScreen().Contains(0, 0));
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

// Verifies that the launcher does not reopen when it's closed by a click on the
// home button.
TEST_F(AppListBubbleTest, ClickOnHomeButtonClosesBubble) {
  AppListBubble* bubble = GetAppListBubble();
  bubble->Show(GetPrimaryDisplay().id());

  // Click the home button.
  WidgetDestroyedWaiter waiter(bubble->bubble_widget_for_test());
  HomeButton* button = GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  GetEventGenerator()->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

}  // namespace
}  // namespace ash
