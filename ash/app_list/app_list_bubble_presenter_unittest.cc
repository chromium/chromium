// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

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
#include "base/test/metrics/histogram_tester.h"
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

class AppListBubblePresenterTest : public AshTestBase {
 public:
  AppListBubblePresenterTest() {
    scoped_features_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~AppListBubblePresenterTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    // Use a realistic screen size so the default size bubble will fit.
    UpdateDisplay("1366x768");
  }

  // Returns the presenter instance. Use this instead of creating a new
  // presenter instance in each test to avoid situations where two bubbles
  // exist at the same time (the per-test one and the "production" one).
  AppListBubblePresenter* GetBubblePresenter() {
    return Shell::Get()->app_list_controller()->bubble_presenter_for_test();
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AppListBubblePresenterTest, ShowOpensOneWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubblePresenterTest, ShowRecordsCreationTimeHistogram) {
  base::HistogramTester histogram_tester;
  AppListBubblePresenter* presenter = GetBubblePresenter();

  presenter->Show(GetPrimaryDisplay().id());
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleCreationTime", 1);

  presenter->Dismiss();
  presenter->Show(GetPrimaryDisplay().id());
  histogram_tester.ExpectTotalCount("Apps.AppListBubbleCreationTime", 2);
}

TEST_F(AppListBubblePresenterTest, DismissClosesWidget) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  WidgetDestroyedWaiter waiter(presenter->bubble_widget_for_test());
  presenter->Dismiss();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubblePresenterTest, DismissWhenNotShowingDoesNotCrash) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  EXPECT_FALSE(presenter->IsShowing());

  presenter->Dismiss();
  // No crash.
}

TEST_F(AppListBubblePresenterTest, ToggleOpensOneWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Toggle(GetPrimaryDisplay().id());

  EXPECT_EQ(1u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubblePresenterTest, ToggleClosesWidgetInAppListContainer) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Toggle(GetPrimaryDisplay().id());

  WidgetDestroyedWaiter waiter(presenter->bubble_widget_for_test());
  presenter->Toggle(GetPrimaryDisplay().id());
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

TEST_F(AppListBubblePresenterTest, BubbleIsNotShowingByDefault) {
  AppListBubblePresenter* presenter = GetBubblePresenter();

  EXPECT_FALSE(presenter->IsShowing());
}

TEST_F(AppListBubblePresenterTest, BubbleIsShowingAfterShow) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  EXPECT_TRUE(presenter->IsShowing());
}

TEST_F(AppListBubblePresenterTest, BubbleIsNotShowingAfterDismiss) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());
  presenter->Dismiss();

  EXPECT_FALSE(presenter->IsShowing());
}

TEST_F(AppListBubblePresenterTest, DoesNotCrashWhenNativeWidgetDestroyed) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

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

TEST_F(AppListBubblePresenterTest, ClickInTopLeftOfScreenClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  Widget* widget = presenter->bubble_widget_for_test();
  WidgetDestroyedWaiter waiter(widget);
  ASSERT_FALSE(widget->GetWindowBoundsInScreen().Contains(0, 0));
  GetEventGenerator()->MoveMouseTo(0, 0);
  GetEventGenerator()->ClickLeftButton();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

// Verifies that the launcher does not reopen when it's closed by a click on the
// home button.
TEST_F(AppListBubblePresenterTest, ClickOnHomeButtonClosesBubble) {
  AppListBubblePresenter* presenter = GetBubblePresenter();
  presenter->Show(GetPrimaryDisplay().id());

  // Click the home button.
  WidgetDestroyedWaiter waiter(presenter->bubble_widget_for_test());
  HomeButton* button = GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  GetEventGenerator()->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  waiter.Wait();

  EXPECT_EQ(0u, NumberOfWidgetsInAppListContainer());
}

}  // namespace
}  // namespace ash
