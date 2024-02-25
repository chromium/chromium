// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/bubble_event_filter.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Parameterized by mouse events vs. touch events.
class BubbleEventFilterTest : public AshTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  BubbleEventFilterTest() = default;
  BubbleEventFilterTest(const BubbleEventFilterTest&) = delete;
  BubbleEventFilterTest& operator=(const BubbleEventFilterTest&) = delete;
  ~BubbleEventFilterTest() override = default;

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

INSTANTIATE_TEST_SUITE_P(MouseOrTouch, BubbleEventFilterTest, testing::Bool());

TEST_P(BubbleEventFilterTest, ClickOutsideWidgetRunsCallback) {
  int callback_count = 0;
  auto callback = base::BindLambdaForTesting(
      [&](const ui::LocatedEvent& event) { ++callback_count; });
  BubbleEventFilter filter(widget_.get(), view_, callback);

  // Click outside the widget.
  gfx::Point point_outside_widget = widget_->GetWindowBoundsInScreen().origin();
  point_outside_widget.Offset(-1, -1);
  ClickOrTapAt(point_outside_widget);

  EXPECT_EQ(callback_count, 1);
}

TEST_P(BubbleEventFilterTest, ClickInsideWidgetDoesNotRunCallback) {
  bool callback_ran = false;
  auto callback = base::BindLambdaForTesting(
      [&](const ui::LocatedEvent& event) { callback_ran = true; });
  BubbleEventFilter filter(widget_.get(), view_, callback);

  // Click inside the widget.
  ClickOrTapAt(widget_->GetWindowBoundsInScreen().CenterPoint());

  EXPECT_FALSE(callback_ran);
}

TEST_P(BubbleEventFilterTest, ClickInsideViewDoesNotRunCallback) {
  bool callback_ran = false;
  auto callback = base::BindLambdaForTesting(
      [&](const ui::LocatedEvent& event) { callback_ran = true; });
  BubbleEventFilter filter(widget_.get(), view_, callback);

  // Click inside the view.
  ClickOrTapAt(view_->GetBoundsInScreen().CenterPoint());

  EXPECT_FALSE(callback_ran);
}

}  // namespace ash
