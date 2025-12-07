// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_bubble_event_filter.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_widget_builder.h"
#include "ui/views/test/widget_test.h"

namespace ash {
namespace {

using QuickInsertBubbleEventFilterTest = AshTestBase;

TEST_F(QuickInsertBubbleEventFilterTest, ClickingOnWidgetDoesNotCloseWidget) {
  auto widget = views::test::TestWidgetBuilder()
                    .SetBounds({10, 10, 100, 100})
                    .BuildClientOwnsWidget();
  QuickInsertBubbleEventFilter filter(widget.get());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(widget->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_FALSE(widget->IsClosed());
}

TEST_F(QuickInsertBubbleEventFilterTest,
       ClickingOnChildWidgetDoesNotCloseWidget) {
  auto widget = views::test::TestWidgetBuilder()
                    .SetBounds({10, 10, 100, 100})
                    .BuildClientOwnsWidget();
  auto child = views::test::TestWidgetBuilder()
                   .SetBounds({1000, 1000, 100, 100})
                   .SetParent(widget->GetNativeWindow())
                   .SetActivatable(false)
                   .BuildClientOwnsWidget();
  views::Widget::ReparentNativeView(child->GetNativeView(),
                                    widget->GetNativeView());
  QuickInsertBubbleEventFilter filter(widget.get());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(child->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();

  EXPECT_FALSE(widget->IsClosed());
}

TEST_F(QuickInsertBubbleEventFilterTest, ClickingOutsideWidgetClosesWidget) {
  auto widget = views::test::TestWidgetBuilder()
                    .SetBounds({10, 10, 100, 100})
                    .BuildClientOwnsWidget();
  QuickInsertBubbleEventFilter filter(widget.get());

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
