// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/status_area_overflow_button_tray.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/run_loop.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"

namespace ash {

class StatusAreaOverflowButtonTrayTest : public AshTestBase {
 public:
  StatusAreaOverflowButtonTrayTest() = default;
  ~StatusAreaOverflowButtonTrayTest() override = default;

  void SetUp() override { AshTestBase::SetUp(); }

  void TapButton() {
    ui::GestureEvent tap_event =
        ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    GetTray()->PerformAction(tap_event);
  }

  StatusAreaOverflowButtonTray* GetTray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->overflow_button_tray();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusAreaOverflowButtonTrayTest);
};

TEST_F(StatusAreaOverflowButtonTrayTest, ToggleExpanded) {
  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_EXPAND, GetTray()->state());
  TapButton();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_COLLAPSE,
            GetTray()->state());
  TapButton();
  EXPECT_EQ(StatusAreaOverflowButtonTray::CLICK_TO_EXPAND, GetTray()->state());
}

}  // namespace ash
