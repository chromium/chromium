// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button.h"

#include <memory>
#include <string>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"

namespace ash {

ui::GestureEvent CreateGestureEvent(ui::GestureEventDetails details) {
  return ui::GestureEvent(0, 0, ui::EF_NONE, base::TimeTicks(), details);
}

class HomeButtonTest : public AshTestBase,
                       public testing::WithParamInterface<bool> {
 public:
  HomeButtonTest() = default;
  ~HomeButtonTest() override = default;

  // AshTestBase:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kShelfHotseat);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kShelfHotseat);
    }

    AshTestBase::SetUp();
  }

  void SendGestureEvent(ui::GestureEvent* event) {
    GetPrimaryShelf()->shelf_widget()->GetHomeButton()->OnGestureEvent(event);
  }

  void SendGestureEventToSecondaryDisplay(ui::GestureEvent* event) {
    // Add secondary display.
    UpdateDisplay("1+1-1000x600,1002+0-600x400");
    // Send the gesture event to the secondary display.
    Shelf::ForWindow(Shell::GetAllRootWindows()[1])
        ->shelf_widget()
        ->GetHomeButton()
        ->OnGestureEvent(event);
  }

  const HomeButton* home_button() const {
    return GetPrimaryShelf()->shelf_widget()->GetHomeButton();
  }

  AssistantState* assistant_state() const { return AssistantState::Get(); }

  PrefService* prefs() {
    return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HomeButtonTest);
};

// The parameter indicates whether the kShelfHotseat feature is enabled.
INSTANTIATE_TEST_SUITE_P(, HomeButtonTest, testing::Bool());

TEST_P(HomeButtonTest, SwipeUpToOpenFullscreenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  // Start the drags from the center of the shelf.
  const ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  gfx::Point start =
      gfx::Point(shelf_view->width() / 2, shelf_view->height() / 2);
  views::View::ConvertPointToScreen(shelf_view, &start);
  // Swiping up less than the threshold should trigger a peeking app list.
  gfx::Point end = start;
  end.set_y(shelf->GetIdealBounds().bottom() -
            AppListView::kDragSnapToPeekingThreshold + 10);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Swiping above the threshold should trigger a fullscreen app list.
  end.set_y(shelf->GetIdealBounds().bottom() -
            AppListView::kDragSnapToPeekingThreshold - 10);
  GetEventGenerator()->GestureScrollSequence(
      start, end, base::TimeDelta::FromMilliseconds(100), 4 /* steps */);
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);
}

TEST_P(HomeButtonTest, ClickToOpenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());

  gfx::Point center = home_button()->GetCenterPoint();
  views::View::ConvertPointToScreen(home_button(), &center);
  GetEventGenerator()->MoveMouseTo(center);

  // Click on the home button should toggle the app list.
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kPeeking);
  GetEventGenerator()->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  // Shift-click should open the app list in fullscreen.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Another shift-click should close the app list.
  GetEventGenerator()->set_flags(ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ClickLeftButton();
  GetEventGenerator()->set_flags(0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);
}

TEST_P(HomeButtonTest, ButtonPositionInTabletMode) {
  // Finish all setup tasks. In particular we want to finish the
  // GetSwitchStates post task in (Fake)PowerManagerClient which is triggered
  // by TabletModeController otherwise this will cause tablet mode to exit
  // while we wait for animations in the test.
  base::RunLoop().RunUntilIdle();

  ShelfViewTestAPI test_api(GetPrimaryShelf()->GetShelfViewForTesting());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  // When hotseat is enabled, home button position changes between in-app shelf
  // and home shelf, so test in-app when hotseat is enabled.
  if (GetParam()) {
    // Wait for the navigation widget's animation.
    test_api.RunMessageLoopUntilAnimationsDone(
        GetPrimaryShelf()
            ->shelf_widget()
            ->navigation_widget()
            ->get_bounds_animator_for_testing());

    EXPECT_EQ(home_button()->bounds().x(), 0);

    // Switch to in-app shelf.
    std::unique_ptr<views::Widget> widget = CreateTestWidget();
  }

  // Wait for the navigation widget's animation.
  test_api.RunMessageLoopUntilAnimationsDone(
      GetPrimaryShelf()
          ->shelf_widget()
          ->navigation_widget()
          ->get_bounds_animator_for_testing());
  EXPECT_GT(home_button()->bounds().x(), 0);

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  test_api.RunMessageLoopUntilAnimationsDone(
      GetPrimaryShelf()
          ->shelf_widget()
          ->navigation_widget()
          ->get_bounds_animator_for_testing());

  // Visual space around the home button is set at the widget level.
  EXPECT_EQ(0, home_button()->bounds().x());
}

TEST_P(HomeButtonTest, LongPressGesture) {
  ui::ScopedAnimationDurationScaleMode animation_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Enable the Assistant in system settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);
  assistant_state()->NotifyFeatureAllowed(
      mojom::AssistantAllowedState::ALLOWED);
  assistant_state()->NotifyStatusChanged(mojom::AssistantState::READY);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());

  Shell::Get()->assistant_controller()->ui_controller()->CloseUi(
      AssistantExitPoint::kUnspecified);
  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());
}

TEST_P(HomeButtonTest, LongPressGestureWithSecondaryUser) {
  // Disallowed by secondary user.
  assistant_state()->NotifyFeatureAllowed(
      mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER);

  // Enable the Assistant in system settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  // The Assistant is disabled for secondary user.
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());
}

TEST_P(HomeButtonTest, LongPressGestureWithSettingsDisabled) {
  // Simulate two user with primary user as active.
  CreateUserSessions(2);

  // Simulate a user who has already completed setup flow, but disabled the
  // Assistant in settings.
  prefs()->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
  assistant_state()->NotifyFeatureAllowed(
      mojom::AssistantAllowedState::ALLOWED);

  ui::GestureEvent long_press =
      CreateGestureEvent(ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  SendGestureEvent(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());

  // Test long press gesture on secondary display.
  SendGestureEventToSecondaryDisplay(&long_press);
  EXPECT_NE(AssistantVisibility::kVisible, Shell::Get()
                                               ->assistant_controller()
                                               ->ui_controller()
                                               ->model()
                                               ->visibility());
}

}  // namespace ash
