// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_event_filter.h"

#include <memory>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Parameterized by mouse events vs. touch events.
class AppListBubbleEventFilterTest : public AshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  AppListBubbleEventFilterTest() = default;
  AppListBubbleEventFilterTest(const AppListBubbleEventFilterTest&) = delete;
  AppListBubbleEventFilterTest& operator=(const AppListBubbleEventFilterTest&) =
      delete;
  ~AppListBubbleEventFilterTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    widget_ = TestWidgetBuilder()
                  .SetBounds({10, 10, 100, 100})
                  .SetShow(true)
                  .BuildOwnsNativeWidget();
    // Create a separate Widget to host the View. A View must live in a Widget
    // to have valid screen coordinates.
    view_holder_widget_ = TestWidgetBuilder()
                              .SetBounds({500, 500, 100, 100})
                              .SetShow(true)
                              .BuildOwnsNativeWidget();
    view_ = view_holder_widget_->client_view()->AddChildView(
        std::make_unique<views::View>());
    view_->SetBoundsRect({0, 0, 32, 32});
  }

  // Generates a click or a tap based on test parameterization.
  void ClickOrTapAt(gfx::Point point_in_screen) {
    auto* generator = GetEventGenerator();
    if (GetParam()) {
      generator->MoveMouseTo(point_in_screen);
      generator->ClickLeftButton();
    } else {
      generator->GestureTapAt(point_in_screen);
    }
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::Widget> view_holder_widget_;
  raw_ptr<views::View> view_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(MouseOrTouch,
                         AppListBubbleEventFilterTest,
                         testing::Bool());

TEST_P(AppListBubbleEventFilterTest, ClickOnStatusAreaDoesNotRunCallback) {
  int callback_count = 0;
  auto callback = base::BindLambdaForTesting(
      [&](const ui::LocatedEvent& event) { ++callback_count; });
  AppListBubbleEventFilter filter(widget_.get(), view_, callback);

  // Click on the status area widget should not trigger the callback.
  auto* status_area =
      Shell::Get()->GetPrimaryRootWindowController()->GetStatusAreaWidget();
  ClickOrTapAt(status_area->GetWindowBoundsInScreen().CenterPoint());

  EXPECT_EQ(callback_count, 0);
}

}  // namespace
}  // namespace ash
