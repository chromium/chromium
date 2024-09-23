// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_bubble_event_filter.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/test_widget_builder.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

using PickerBubbleEventFilterTest = AshTestBase;

TEST_F(PickerBubbleEventFilterTest, ClickingOnWidgetDoesNotCloseWidget) {
  auto widget =
      TestWidgetBuilder().SetBounds({10, 10, 100, 100}).BuildClientOwnsWidget();
  PickerBubbleEventFilter filter(widget.get());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(widget->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_FALSE(widget->IsClosed());
}

TEST_F(PickerBubbleEventFilterTest, ClickingOnChildWidgetDoesNotCloseWidget) {
  auto widget =
      TestWidgetBuilder().SetBounds({10, 10, 100, 100}).BuildClientOwnsWidget();
  auto child = TestWidgetBuilder()
                   .SetBounds({1000, 1000, 100, 100})
                   .SetParent(widget->GetNativeWindow())
                   .SetActivatable(false)
                   .BuildClientOwnsWidget();
  views::Widget::ReparentNativeView(child->GetNativeView(),
                                    widget->GetNativeView());
  PickerBubbleEventFilter filter(widget.get());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(child->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_FALSE(widget->IsClosed());
}

TEST_F(PickerBubbleEventFilterTest, ClickingOutsideWidgetClosesWidget) {
  auto widget =
      TestWidgetBuilder().SetBounds({10, 10, 100, 100}).BuildClientOwnsWidget();
  PickerBubbleEventFilter filter(widget.get());

  // Click above the top left corner.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(widget->GetWindowBoundsInScreen().origin() -
                         gfx::Vector2d(0, 1));
  generator->ClickLeftButton();

  ASSERT_TRUE(widget->IsClosed());
  views::test::WidgetDestroyedWaiter(widget.get()).Wait();
}

}  // namespace
}  // namespace ash
