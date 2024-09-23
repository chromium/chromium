// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/utils/haptics_util.h"

#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/utility/haptics_tracking_test_input_controller.h"
#include "ash/wm/desks/desk_animation_impl.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/workspace/workspace_window_resizer_test_api.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view_test_api.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "ui/aura/window.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace ash {

using ui::HapticTouchpadEffect;
using ui::HapticTouchpadEffectStrength;

using HapticsUtilTest = AshTestBase;

// Test haptic feedback with all effect/strength combinations.
TEST_F(HapticsUtilTest, HapticFeedbackBasic) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();

  std::vector<HapticTouchpadEffect> effects = {
      HapticTouchpadEffect::kSnap,    HapticTouchpadEffect::kKnock,
      HapticTouchpadEffect::kTick,    HapticTouchpadEffect::kPress,
      HapticTouchpadEffect::kRelease,
  };
  std::vector<HapticTouchpadEffectStrength> strengths = {
      HapticTouchpadEffectStrength::kLow,
      HapticTouchpadEffectStrength::kMedium,
      HapticTouchpadEffectStrength::kHigh,
  };

  for (HapticTouchpadEffect effect : effects) {
    for (HapticTouchpadEffectStrength strength : strengths) {
      for (int count = 0; count < 16; count++) {
        chromeos::haptics_util::PlayHapticTouchpadEffect(effect, strength);
        EXPECT_EQ(count + 1,
                  input_controller->GetSentHapticCount(effect, strength));
      }
    }
  }
}

// Test haptic feedback for normal window snapping. This covers drag to snap
// primary/secondary/maximize.
TEST_F(HapticsUtilTest, HapticFeedbackForNormalWindowSnap) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();

  UpdateDisplay("800x600");
  gfx::Rect bounds(200, 200, 300, 300);
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Each element in the vector represents a test case. The first in the pair is
  // the drag target point, which is used to trigger window snapping, and the
  // second is the expected window bounds after drag.
  std::vector<std::pair<gfx::Point, gfx::Rect>> test_cases = {
      {{0, 220}, {0, 0, 400, 552}},
      {{800, 220}, {400, 0, 400, 552}},
      {{350, 0}, {0, 0, 800, 552}},
  };

  // Drag by touch should not trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    window->SetBounds(bounds);
    gfx::Point start =
        window->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 20);
    event_generator->set_current_screen_location(start);
    event_generator->PressTouch();
    event_generator->MoveTouch(test_case.first);
    EXPECT_EQ(0, input_controller->GetSentHapticCount(
                     HapticTouchpadEffect::kSnap,
                     HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseTouch();
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }

  // Drag by touchpad/mouse should trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    window->SetBounds(bounds);
    gfx::Point start =
        window->GetBoundsInScreen().top_center() + gfx::Vector2d(0, 20);
    event_generator->set_current_screen_location(start);
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(test_case.first);
    auto& dwell_countdown_timer =
        WorkspaceWindowResizerTestApi().GetDwellCountdownTimer();
    if (dwell_countdown_timer.IsRunning()) {
      dwell_countdown_timer.FireNow();
    }
    EXPECT_EQ((int)i + 1, input_controller->GetSentHapticCount(
                              HapticTouchpadEffect::kSnap,
                              HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseLeftButton();
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }
}

// Test haptic feedback for overview window snapping. This covers drag to split
// in overview.
TEST_F(HapticsUtilTest, HapticFeedbackForOverviewWindowSnap) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();
  OverviewController* overview_controller = Shell::Get()->overview_controller();

  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(200, 200, 300, 300));
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Each element in the vector represents a test case. The first in the pair is
  // the drag target point, which is used to trigger window snapping, and the
  // second is the expected window bounds after drag.
  std::vector<std::pair<gfx::Point, gfx::Rect>> test_cases = {
      {{0, 300}, {0, 0, 400, 552}},
      {{800, 300}, {400, 0, 400, 552}},
  };

  // Drag by touch should not trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    EnterOverview();
    auto* overview_item =
        overview_controller->overview_session()->GetOverviewItemForWindow(
            window.get());

    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->PressTouch();
    event_generator->MoveTouch(test_case.first);
    EXPECT_TRUE(overview_controller->InOverviewSession());
    EXPECT_EQ(0, input_controller->GetSentHapticCount(
                     HapticTouchpadEffect::kSnap,
                     HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseTouch();
    EXPECT_FALSE(overview_controller->InOverviewSession());
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }

  // Drag by touchpad/mouse should trigger haptic feedback.
  for (size_t i = 0; i < test_cases.size(); i++) {
    std::pair<gfx::Point, gfx::Rect> test_case = test_cases[i];
    EnterOverview();
    auto* overview_item =
        overview_controller->overview_session()->GetOverviewItemForWindow(
            window.get());

    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(test_case.first);
    EXPECT_TRUE(overview_controller->InOverviewSession());
    EXPECT_EQ((int)i + 1, input_controller->GetSentHapticCount(
                              HapticTouchpadEffect::kSnap,
                              HapticTouchpadEffectStrength::kMedium));
    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(overview_controller->InOverviewSession());
    EXPECT_EQ(test_case.second, window->GetBoundsInScreen());
  }
}

// Test haptic feedback for off limits desk switching, e.g. swiping left from
// the first desk and swiping right from the last desk.
TEST_F(HapticsUtilTest, HapticFeedbackForDeskSwitchingOffLimits) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();
  auto* desk_controller = DesksController::Get();

  // Make sure to start with two desks.
  NewDesk();
  EXPECT_EQ(2u, desk_controller->desks().size());
  EXPECT_EQ(0, desk_controller->GetActiveDeskIndex());
  EXPECT_EQ(
      input_controller->GetSentHapticCount(
          HapticTouchpadEffect::kKnock, HapticTouchpadEffectStrength::kMedium),
      0);

  // Swipe from `desk 0` to `desk 1` should not trigger `kKnock` effect.
  ScrollToSwitchDesks(/*scroll_left=*/false, GetEventGenerator());
  EXPECT_EQ(1, desk_controller->GetActiveDeskIndex());
  EXPECT_EQ(0, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));

  // Swipe from `desk 1` to right should trigger `kKnock` effect.
  ScrollToSwitchDesks(/*scroll_left=*/false,
                      /*event_generator=*/GetEventGenerator());
  EXPECT_EQ(1, desk_controller->GetActiveDeskIndex());
  EXPECT_EQ(1, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));
}

// Tests that haptics are sent when doing a continuous touchpad gesture to
// switch desks. They are expected to be sent if we hit the edge, or when the
// visible desk changes.
TEST_F(HapticsUtilTest, HapticFeedbackForContinuousDesksSwitching) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();

  // Add three desks for a total of four.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  // Create a standalone animation object. This is the same object that gets
  // created when swiping with 4 fingers, but mocking 4 fingers swipes is harder
  // to control in a test with all the async operations and touchpad unit
  // conversions.
  DeskActivationAnimation animation(desks_controller, 0, 1,
                                    DesksSwitchSource::kDeskSwitchTouchpad,
                                    /*update_window_activation=*/false);
  animation.set_skip_notify_controller_on_animation_finished_for_testing(true);
  animation.Launch();

  // Wait for the ending screenshot to be taken.
  WaitUntilEndingScreenshotTaken(&animation);

  EXPECT_EQ(0, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));
  EXPECT_EQ(0, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kTick,
                   HapticTouchpadEffectStrength::kMedium));

  // Swipe enough so that our third and fourth desk screenshots are taken, and
  // then swipe so that the fourth desk is fully shown. There should be 3
  // visible desk changes in total, which means 3 tick haptic events sent.
  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  WaitUntilEndingScreenshotTaken(&animation);

  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  WaitUntilEndingScreenshotTaken(&animation);

  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  EXPECT_EQ(3, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kTick,
                   HapticTouchpadEffectStrength::kMedium));

  // Try doing a full swipe to the right. Test that a knock haptic event is sent
  // because we are at the edge.
  animation.UpdateSwipeAnimation(-kTouchpadSwipeLengthForDeskChange);
  EXPECT_EQ(1, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));

  // Swipe 3 times to the left. We move from the fourth desk as the visible desk
  // to the first desk, so there should be three more tick haptic events.
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange);
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange);
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange);
  EXPECT_EQ(6, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kTick,
                   HapticTouchpadEffectStrength::kMedium));

  // Swipe to the left while at the first desk. Tests that another haptic event
  // is sent because we are at the edge.
  animation.UpdateSwipeAnimation(kTouchpadSwipeLengthForDeskChange);
  EXPECT_EQ(2, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kKnock,
                   HapticTouchpadEffectStrength::kMedium));
}

// Tests that haptics are sent when dragging a window/desk in overview.
TEST_F(HapticsUtilTest, HapticFeedbackForDragAndDrop) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();
  OverviewController* overview_controller = Shell::Get()->overview_controller();

  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Add three desks for a total of two.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  // Drag a window in overview. Test that kTick feedback is sent.
  EnterOverview();
  auto* overview_item =
      overview_controller->overview_session()->GetOverviewItemForWindow(
          window.get());
  const gfx::RectF bounds_f = overview_item->target_bounds();
  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(bounds_f.CenterPoint()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(bounds_f.right_center()));
  EXPECT_EQ(1, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kTick,
                   HapticTouchpadEffectStrength::kMedium));
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Drag a desk in overview. Test that kTick feedback is sent.
  const gfx::Rect bounds = overview_controller->overview_session()
                               ->grid_list()
                               .front()
                               ->desks_bar_view()
                               ->mini_views()
                               .front()
                               ->bounds();
  event_generator->set_current_screen_location(bounds.CenterPoint());
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(bounds.right_center());
  EXPECT_EQ(2, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kTick,
                   HapticTouchpadEffectStrength::kMedium));
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ExitOverview();
}

TEST_F(HapticsUtilTest, HapticFeedbackForMultitaskMenu) {
  auto input_controller =
      std::make_unique<HapticsTrackingTestInputController>();

  std::unique_ptr<aura::Window> window = CreateAppWindow();

  // Show the clamshell multitask menu via hover. Test that kSnap feedback is
  // sent.
  chromeos::MultitaskMenu* multitask_menu = ShowAndWaitMultitaskMenuForWindow(
      window.get(), chromeos::MultitaskMenuEntryType::kFrameSizeButtonHover);
  ASSERT_TRUE(multitask_menu);
  EXPECT_EQ(1, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kSnap,
                   HapticTouchpadEffectStrength::kMedium));

  // Test that the kSnap feedback is sent whenever we hover over one of the
  // multitask menu buttons.
  auto* event_generator = GetEventGenerator();
  chromeos::MultitaskMenuViewTestApi test_api(
      multitask_menu->multitask_menu_view());
  event_generator->MoveMouseTo(
      test_api.GetFloatButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(2, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kSnap,
                   HapticTouchpadEffectStrength::kMedium));

  event_generator->MoveMouseTo(
      test_api.GetFullButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(3, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kSnap,
                   HapticTouchpadEffectStrength::kMedium));

  chromeos::SplitButtonView* half_button = test_api.GetHalfButton();
  event_generator->MoveMouseTo(
      half_button->GetLeftTopButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(4, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kSnap,
                   HapticTouchpadEffectStrength::kMedium));

  event_generator->MoveMouseTo(
      half_button->GetRightBottomButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(5, input_controller->GetSentHapticCount(
                   HapticTouchpadEffect::kSnap,
                   HapticTouchpadEffectStrength::kMedium));
}

}  // namespace ash
