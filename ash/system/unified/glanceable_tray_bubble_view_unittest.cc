// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class GlanceableTrayBubbleViewTest : public AshTestBase {
 public:
  CalendarView* GetCalendarView() {
    auto* date_tray = Shell::GetPrimaryRootWindowController()
                          ->shelf()
                          ->GetStatusAreaWidget()
                          ->date_tray();

    // Ensures the `GlanceableTrayBubble` exists before getting `CalendarView`.
    date_tray->ShowGlanceableBubble(/*from_keyboard=*/false);
    CHECK(date_tray->glanceables_bubble_for_test());

    return date_tray->glanceables_bubble_for_test()->GetCalendarView();
  }
};

TEST_F(GlanceableTrayBubbleViewTest, CalendarViewHeight) {
  // Sets the display height to be greater than 800dp, then the calendar height
  // should be 368dp.
  UpdateDisplay("1200x1000");
  EXPECT_EQ(GetCalendarView()->height(), 368);

  // Sets the display height to be less than 800dp, then the calendar height
  // should be 340dp.
  UpdateDisplay("1200x600");
  EXPECT_EQ(GetCalendarView()->height(), 340);

  // TODO(b/312320532): Add a test when the display height is less than 340dp.
}

}  // namespace ash
