// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class OverviewWindowDragHistogramTest : public AshTestBase {
 public:
  OverviewWindowDragHistogramTest() = default;
  ~OverviewWindowDragHistogramTest() override = default;
  OverviewWindowDragHistogramTest(const OverviewWindowDragHistogramTest&) =
      delete;
  OverviewWindowDragHistogramTest& operator=(
      const OverviewWindowDragHistogramTest&) = delete;

  void SetUp() override {
    AshTestBase::SetUp();
    window_ = CreateAppWindow();
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  void AddSecondDesk() {
    DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, DesksController::Get()->desks().size());

    // Give the second desk a name. The desk name gets exposed as the accessible
    // name. And the focusable views that are painted in these tests will fail
    // the accessibility paint checker checks if they lack an accessible name.
    DesksController::Get()->desks()[1]->SetName(u"Desk 2", false);
  }

 protected:
  void EnterTablet() {
    TabletModeControllerTestApi().DetachAllMice();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  }

  gfx::Point EnterOverviewAndGetItemCenterPoint() {
    EnterOverview();
    return gfx::ToRoundedPoint(
        GetOverviewItemForWindow(window_.get())->target_bounds().CenterPoint());
  }

  gfx::Point GetSecondDeskMiniViewCenterPoint(size_t grid_index) {
    return GetOverviewSession()
        ->grid_list()[grid_index]
        ->desks_bar_view()
        ->mini_views()[1u]
        ->GetBoundsInScreen()
        .CenterPoint();
  }

  void EnterTabletAndOverviewAndLongPressItem() {
    // Temporarily reconfigure gestures so the long press takes 2 milliseconds.
    ui::GestureConfiguration* gesture_config =
        ui::GestureConfiguration::GetInstance();
    const int old_long_press_time_in_ms =
        gesture_config->long_press_time_in_ms();
    const base::TimeDelta old_short_press_time =
        gesture_config->short_press_time();
    const int old_show_press_delay_in_ms =
        gesture_config->show_press_delay_in_ms();
    gesture_config->set_long_press_time_in_ms(1);
    gesture_config->set_short_press_time(base::Milliseconds(1));
    gesture_config->set_show_press_delay_in_ms(1);

    EnterTablet();
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        EnterOverviewAndGetItemCenterPoint());
    generator->PressTouch();
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
    run_loop.Run();

    gesture_config->set_long_press_time_in_ms(old_long_press_time_in_ms);
    gesture_config->set_short_press_time(old_short_press_time);
    gesture_config->set_show_press_delay_in_ms(old_show_press_delay_in_ms);
  }

  void EnterOverviewAndSwipeItemDown(unsigned int distance) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        EnterOverviewAndGetItemCenterPoint());
    generator->PressTouch();
    // Use small increments to avoid flinging.
    for (unsigned int i = 0u; i < distance; ++i)
      generator->MoveTouchBy(0, 1);
    generator->ReleaseTouch();
  }

  void EnterOverviewAndFlingItemDown() {
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->set_current_screen_location(
        EnterOverviewAndGetItemCenterPoint());
    generator->PressMoveAndReleaseTouchBy(0, 20);
  }

  void ExpectDragRecorded(OverviewDragAction action) {
    histogram_tester_.ExpectUniqueSample("Ash.Overview.WindowDrag.Workflow",
                                         action, 1);
  }

  void ExpectNoDragRecorded() {
    histogram_tester_.ExpectTotalCount("Ash.Overview.WindowDrag.Workflow", 0);
  }

 private:
  base::HistogramTester histogram_tester_;
  std::unique_ptr<aura::Window> window_;
};

TEST_F(OverviewWindowDragHistogramTest, ToGridSameDisplayClamshellMouse) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseBy(5, 0);
  ExpectDragRecorded(OverviewDragAction::kToGridSameDisplayClamshellMouse);
}

TEST_F(OverviewWindowDragHistogramTest, ToGridSameDisplayClamshellTouch) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(EnterOverviewAndGetItemCenterPoint());
  generator->PressMoveAndReleaseTouchBy(20, 0);
  ExpectDragRecorded(OverviewDragAction::kToGridSameDisplayClamshellTouch);
}

TEST_F(OverviewWindowDragHistogramTest, ToDeskSameDisplayClamshellMouse) {
  AddSecondDesk();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  ExpectDragRecorded(OverviewDragAction::kToDeskSameDisplayClamshellMouse);
}

TEST_F(OverviewWindowDragHistogramTest, ToDeskSameDisplayClamshellTouch) {
  AddSecondDesk();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(EnterOverviewAndGetItemCenterPoint());
  generator->PressMoveAndReleaseTouchTo(
      GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  ExpectDragRecorded(OverviewDragAction::kToDeskSameDisplayClamshellTouch);
}

TEST_F(OverviewWindowDragHistogramTest, ToSnapSameDisplayClamshellMouse) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(0, 300);
  ExpectDragRecorded(OverviewDragAction::kToSnapSameDisplayClamshellMouse);
}

TEST_F(OverviewWindowDragHistogramTest, ToSnapSameDisplayClamshellTouch) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(EnterOverviewAndGetItemCenterPoint());
  generator->PressMoveAndReleaseTouchTo(0, 300);
  ExpectDragRecorded(OverviewDragAction::kToSnapSameDisplayClamshellTouch);
}

TEST_F(OverviewWindowDragHistogramTest, SwipeToCloseSuccessfulClamshellTouch) {
  EnterOverviewAndSwipeItemDown(180u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseSuccessfulClamshellTouch);
}

TEST_F(OverviewWindowDragHistogramTest, SwipeToCloseCanceledClamshellTouch) {
  EnterOverviewAndSwipeItemDown(20u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseCanceledClamshellTouch);
}

TEST_F(OverviewWindowDragHistogramTest, SwipeToCloseSuccessfulTabletTouch) {
  EnterTablet();
  EnterOverviewAndSwipeItemDown(180u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseSuccessfulTabletTouch);
}

TEST_F(OverviewWindowDragHistogramTest, SwipeToCloseCanceledTabletTouch) {
  EnterTablet();
  EnterOverviewAndSwipeItemDown(20u);
  ExpectDragRecorded(OverviewDragAction::kSwipeToCloseCanceledTabletTouch);
}

TEST_F(OverviewWindowDragHistogramTest, FlingToCloseClamshellTouch) {
  EnterOverviewAndFlingItemDown();
  ExpectDragRecorded(OverviewDragAction::kFlingToCloseClamshellTouch);
}

TEST_F(OverviewWindowDragHistogramTest, FlingToCloseTabletTouch) {
  EnterTablet();
  EnterOverviewAndFlingItemDown();
  ExpectDragRecorded(OverviewDragAction::kFlingToCloseTabletTouch);
}

TEST_F(OverviewWindowDragHistogramTest, ToGridSameDisplayTabletTouch) {
  EnterTabletAndOverviewAndLongPressItem();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouchBy(20, 0);
  generator->ReleaseTouch();
  ExpectDragRecorded(OverviewDragAction::kToGridSameDisplayTabletTouch);
}

TEST_F(OverviewWindowDragHistogramTest, ToDeskSameDisplayTabletTouch) {
  AddSecondDesk();
  EnterTabletAndOverviewAndLongPressItem();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  generator->ReleaseTouch();
  ExpectDragRecorded(OverviewDragAction::kToDeskSameDisplayTabletTouch);
}

TEST_F(OverviewWindowDragHistogramTest, ToSnapSameDisplayTabletTouch) {
  EnterTabletAndOverviewAndLongPressItem();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(gfx::Point(0, 300));
  generator->ReleaseTouch();
  ExpectDragRecorded(OverviewDragAction::kToSnapSameDisplayTabletTouch);
}

TEST_F(OverviewWindowDragHistogramTest, ToGridSameDisplayTabletMouse) {
  EnterTablet();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseBy(20, 0);
  ExpectNoDragRecorded();
}

TEST_F(OverviewWindowDragHistogramTest, ToDeskSameDisplayTabletMouse) {
  AddSecondDesk();
  EnterTablet();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/0u));
  ExpectNoDragRecorded();
}

TEST_F(OverviewWindowDragHistogramTest, ToSnapSameDisplayTabletMouse) {
  EnterTablet();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->DragMouseTo(0, 300);
  ExpectNoDragRecorded();
}

using OverviewWindowDragHistogramTestMultiDisplayOnly =
    OverviewWindowDragHistogramTest;

TEST_F(OverviewWindowDragHistogramTestMultiDisplayOnly,
       ToGridOtherDisplayClamshellMouse) {
  UpdateDisplay("800x600,800x600");
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  generator->MoveMouseTo(1200, 300);
  generator->ReleaseLeftButton();
  ExpectDragRecorded(OverviewDragAction::kToGridOtherDisplayClamshellMouse);
}

TEST_F(OverviewWindowDragHistogramTestMultiDisplayOnly,
       ToDeskOtherDisplayClamshellMouse) {
  UpdateDisplay("800x600,800x600");
  AddSecondDesk();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  generator->MoveMouseTo(GetSecondDeskMiniViewCenterPoint(/*grid_index=*/1u));
  generator->ReleaseLeftButton();
  ExpectDragRecorded(OverviewDragAction::kToDeskOtherDisplayClamshellMouse);
}

TEST_F(OverviewWindowDragHistogramTestMultiDisplayOnly,
       ToSnapOtherDisplayClamshellMouse) {
  UpdateDisplay("800x600,800x600");
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(EnterOverviewAndGetItemCenterPoint());
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(GetSecondaryDisplay());
  generator->MoveMouseTo(800, 300);
  generator->ReleaseLeftButton();
  ExpectDragRecorded(OverviewDragAction::kToSnapOtherDisplayClamshellMouse);
}

}  // namespace ash
