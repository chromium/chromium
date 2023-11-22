// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/unified/date_tray.h"
#include "ash/system/unified/glanceable_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class GlanceableTrayBubbleViewTest : public AshTestBase {
 public:
  GlanceableTrayBubbleViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlanceablesV2,
                              features::kGlanceablesV2CalendarView},
        /*disabled_features=*/{});
  }

  GlanceableTrayBubbleViewTest(const GlanceableTrayBubbleViewTest&) = delete;
  GlanceableTrayBubbleViewTest& operator=(const GlanceableTrayBubbleViewTest&) =
      delete;
  ~GlanceableTrayBubbleViewTest() override = default;

  CalendarView* GetCalendarView() {
    auto* date_tray = Shell::GetPrimaryRootWindowController()
                          ->shelf()
                          ->GetStatusAreaWidget()
                          ->date_tray();

    // Ensures the `GlanceableTrayBubble` exists before getting `CalendarView`.
    date_tray->ShowGlanceableBubble(/*from_keyboard=*/false);
    CHECK(date_tray->bubble_);

    return date_tray->bubble_->GetCalendarView();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
