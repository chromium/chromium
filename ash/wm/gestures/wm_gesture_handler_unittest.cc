// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/root_window_desk_switch_animator_test_api.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kNumFingersForHighlight = 3;
constexpr int kNumFingersForDesksSwitch = 4;

bool InOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession();
}

const aura::Window* GetHighlightedWindow() {
  return InOverviewSession() ? GetOverviewHighlightedWindow() : nullptr;
}

bool IsNaturalScrollOn() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref->GetBoolean(prefs::kTouchpadEnabled) &&
         pref->GetBoolean(prefs::kNaturalScroll);
}

int GetOffsetX(int offset) {
  // The handler code uses the new directions which is the reverse of the old
  // handler code. Reverse the offset if the ReverseScrollGestures feature is
  // disabled so that the unit tests test the old behavior.
  return features::IsReverseScrollGesturesEnabled() ? offset : -offset;
}

int GetOffsetY(int offset) {
  // The handler code uses the new directions which is the reverse of the old
  // handler code. Reverse the offset if the ReverseScrollGestures feature is
  // disabled so that the unit tests test the old behavior.
  if (!features::IsReverseScrollGesturesEnabled() || IsNaturalScrollOn())
    return -offset;
  return offset;
}

const Desk* GetActiveDesk() {
  return DesksController::Get()->active_desk();
}

const Desk* GetNextDesk() {
  return DesksController::Get()->GetNextDesk();
}

void AddDesk() {
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
}

}  // namespace

class WmGestureHandlerTest : public AshTestBase {
 public:
  WmGestureHandlerTest() = default;
  WmGestureHandlerTest(const WmGestureHandlerTest&) = delete;
  WmGestureHandlerTest& operator=(const WmGestureHandlerTest&) = delete;
  ~WmGestureHandlerTest() override = default;

  void Scroll(float x_offset, float y_offset, int fingers) {
    GetEventGenerator()->ScrollSequence(
        gfx::Point(), base::TimeDelta::FromMilliseconds(5),
        GetOffsetX(x_offset), GetOffsetY(y_offset), /*steps=*/100, fingers);
  }

  void ScrollToSwitchDesks(bool scroll_left) {
    // Scrolling to switch desks with enhanced desk animations is a bit tricky
    // because it involves multiple async operations.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    // Start off with a fling cancel (touchpad start) to start the touchpad
    // swipe sequence.
    base::TimeTicks timestamp = ui::EventTimeForNow();
    ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, gfx::Point(),
                                 timestamp, 0, 0, 0, 0, 0,
                                 kNumFingersForDesksSwitch);
    auto* event_generator = GetEventGenerator();
    event_generator->Dispatch(&fling_cancel);

    // Continue with a large enough scroll to start the desk switch animation.
    // The animation does not start on fling cancel since there is no finger
    // data in production code.
    const base::TimeDelta step_delay = base::TimeDelta::FromMilliseconds(5);
    timestamp += step_delay;
    const int direction = scroll_left ? -1 : 1;
    const int initial_move_x =
        (WmGestureHandler::kContinuousGestureMoveThresholdDp + 5) * direction;
    ui::ScrollEvent initial_move(ui::ET_SCROLL, gfx::Point(), timestamp, 0,
                                 initial_move_x, 0, initial_move_x, 0,
                                 kNumFingersForDesksSwitch);
    event_generator->Dispatch(&initial_move);

    // Wait until the animations ending screenshot has been taken. Otherwise,
    // we will just stay at the initial desk if no screenshot has been taken.
    auto* animation = DesksController::Get()->animation();
    DCHECK(animation);
    auto* desk_switch_animator =
        animation->GetDeskSwitchAnimatorAtIndexForTesting(0);
    base::RunLoop run_loop;
    RootWindowDeskSwitchAnimatorTestApi(desk_switch_animator)
        .SetOnEndingScreenshotTakenCallback(run_loop.QuitClosure());
    run_loop.Run();

    // Send some more move events, enough to shift to the next desk.
    const int steps = 100;
    const float x_offset = direction * WmGestureHandler::kHorizontalThresholdDp;
    float dx = x_offset / steps;
    for (int i = 0; i < steps; ++i) {
      timestamp += step_delay;
      ui::ScrollEvent move(ui::ET_SCROLL, gfx::Point(), timestamp, 0, dx, 0, dx,
                           0, kNumFingersForDesksSwitch);
      event_generator->Dispatch(&move);
    }

    // End the swipe and wait for the animation to finish.
    ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, gfx::Point(),
                                timestamp, 0, x_offset, 0, x_offset, 0,
                                kNumFingersForDesksSwitch);
    DeskSwitchAnimationWaiter animation_finished_waiter;
    event_generator->Dispatch(&fling_start);
    animation_finished_waiter.Wait();
  }

  void MouseWheelScroll(int delta_x, int delta_y, int num_of_times) {
    auto* generator = GetEventGenerator();
    for (int i = 0; i < num_of_times; i++)
      generator->MoveMouseWheel(delta_x, delta_y);
  }
};

// Tests a three fingers upwards scroll gesture to enter and a scroll down to
// exit overview.
TEST_F(WmGestureHandlerTest, VerticalScrolls) {
  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  Scroll(0, long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());

  // Swiping down exits.
  Scroll(0, -long_scroll, 3);
  EXPECT_FALSE(InOverviewSession());
}

// Tests wrong gestures that swiping down to enter and up to exit overview.
TEST_F(WmGestureHandlerTest, WrongVerticalScrolls) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kReverseScrollGestures);

  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;

  // Swiping down cannot enter overview.
  Scroll(0, -long_scroll, 3);
  EXPECT_FALSE(InOverviewSession());

  // Enter overview.
  Scroll(0, long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());

  // Swiping up cannot exit overview.
  Scroll(0, long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());
}

// Tests three or four finger horizontal scroll gesture (depending on flags) to
// move selection left or right.
TEST_F(WmGestureHandlerTest, HorizontalScrollInOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float vertical_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;
  // Enter overview mode as if using an accelerator.
  // Entering overview mode with an upwards three-finger scroll gesture would
  // have the same result (allow selection using horizontal scroll).
  EnterOverview();
  EXPECT_TRUE(InOverviewSession());

  // Scrolls until a window is highlight, ignoring any desks items (if any).
  auto scroll_until_window_highlighted = [this](float x_offset,
                                                float y_offset) {
    do {
      Scroll(GetOffsetX(x_offset), GetOffsetY(y_offset),
             kNumFingersForHighlight);
    } while (!GetHighlightedWindow());
  };

  // Select the first window first.
  scroll_until_window_highlighted(horizontal_scroll, 0);

  // Long scroll right moves selection to the fourth window.
  scroll_until_window_highlighted(horizontal_scroll * 3, 0);
  EXPECT_TRUE(InOverviewSession());

  // Short scroll left moves selection to the third window.
  scroll_until_window_highlighted(-horizontal_scroll, 0);
  EXPECT_TRUE(InOverviewSession());

  // Short scroll left moves selection to the second window.
  scroll_until_window_highlighted(-horizontal_scroll, 0);
  EXPECT_TRUE(InOverviewSession());

  // Swiping down (3 fingers) exits and selects the currently-highlighted
  // window.
  Scroll(0, -vertical_scroll, 3);
  EXPECT_FALSE(InOverviewSession());

  // Second MRU window is selected (i.e. |window4|).
  EXPECT_EQ(window4.get(), window_util::GetActiveWindow());
}

// Tests that a mostly horizontal scroll does not trigger overview.
TEST_F(WmGestureHandlerTest, HorizontalScrolls) {
  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  Scroll(long_scroll + 100, long_scroll, kNumFingersForHighlight);
  EXPECT_FALSE(InOverviewSession());

  Scroll(-long_scroll - 100, long_scroll, kNumFingersForHighlight);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that we only enter overview after a scroll has ended.
TEST_F(WmGestureHandlerTest, EnterOverviewOnScrollEnd) {
  base::TimeTicks timestamp = base::TimeTicks::Now();
  const int num_fingers = 3;
  base::TimeDelta step_delay(base::TimeDelta::FromMilliseconds(5));
  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, gfx::Point(),
                               timestamp, 0, 0, 0, 0, 0, num_fingers);
  GetEventGenerator()->Dispatch(&fling_cancel);

  // Scroll up by 1000px. We are not in overview yet, because the scroll is
  // still ongoing.
  for (int i = 0; i < 100; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::ET_SCROLL, gfx::Point(), timestamp, 0, 0,
                         GetOffsetY(10), 0, GetOffsetY(10), num_fingers);
    GetEventGenerator()->Dispatch(&move);
  }
  ASSERT_FALSE(InOverviewSession());

  timestamp += step_delay;
  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, gfx::Point(),
                              timestamp, 0, 0, GetOffsetY(-10), 0,
                              GetOffsetY(-10), num_fingers);
  GetEventGenerator()->Dispatch(&fling_start);
  EXPECT_TRUE(InOverviewSession());
}

// Test switch desk is disabled when screen is pinned.
TEST_F(WmGestureHandlerTest, LockedModeNoSwitchDesk) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desk_controller->desks().size());
  ASSERT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  // Pin a window to current desk.
  aura::Window* w1 = CreateTestWindowInShellWithId(0);
  wm::ActivateWindow(w1);
  window_util::PinWindow(w1, /*trusted=*/false);
  EXPECT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());

  // Tests that scrolling right won't switch desks when screen is pinned.
  const float long_scroll = WmGestureHandler::kHorizontalThresholdDp;
  Scroll(long_scroll, 0.f, kNumFingersForDesksSwitch);
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());
}

using DesksGestureHandlerTest = WmGestureHandlerTest;

// Tests that a four-finger horizontal scroll will switch desks as expected.
TEST_F(DesksGestureHandlerTest, HorizontalScrolls) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desk_controller->desks().size());
  ASSERT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  // Tests that scrolling right should take us to the next desk.
  ScrollToSwitchDesks(/*scroll_left=*/false);
  EXPECT_EQ(desk_controller->desks()[1].get(), desk_controller->active_desk());

  // Tests that scrolling left should take us to the previous desk.
  ScrollToSwitchDesks(/*scroll_left=*/true);
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  // Tests that since there is no previous desk, we remain on the same desk when
  // scrolling left.
  const float long_scroll = WmGestureHandler::kHorizontalThresholdDp;
  Scroll(-long_scroll, 0.f, kNumFingersForDesksSwitch);
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());
}

// Tests that vertical scrolls and horizontal scrolls that are too small do not
// switch desks.
TEST_F(DesksGestureHandlerTest, NoDeskChanges) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desk_controller->desks().size());
  ASSERT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  const float short_scroll = WmGestureHandler::kHorizontalThresholdDp - 10.f;
  const float long_scroll = WmGestureHandler::kHorizontalThresholdDp;
  // Tests that a short horizontal scroll does not switch desks.
  Scroll(-short_scroll, 0.f, kNumFingersForDesksSwitch);
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  // Tests that a scroll that meets the horizontal requirements, but is mostly
  // vertical does not switch desks.
  Scroll(-long_scroll, long_scroll + 10.f, kNumFingersForDesksSwitch);
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  // Tests that a vertical scroll does not switch desks.
  Scroll(0.f, WmGestureHandler::kVerticalThresholdDp,
         kNumFingersForDesksSwitch);
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());
}

// Tests that touchpad gesture scrolls don't lead to any desk changes when the
// screen is locked.
TEST_F(DesksGestureHandlerTest, NoDeskChangesInLockScreen) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(3u, desk_controller->desks().size());
  ASSERT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  auto* session_controller = Shell::Get()->session_controller();
  session_controller->LockScreen();
  GetSessionControllerClient()->FlushForTest();  // LockScreen is an async call.
  ASSERT_TRUE(session_controller->IsScreenLocked());

  const float long_scroll = WmGestureHandler::kHorizontalThresholdDp * 3;
  Scroll(long_scroll, 0, kNumFingersForDesksSwitch);
  EXPECT_FALSE(desk_controller->AreDesksBeingModified());
  EXPECT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());
}

// Tests that activate highlighted desk when using 3-finger swipes to exit
// overview.
TEST_F(WmGestureHandlerTest, ActivateHighlightedDeskWithVerticalScroll) {
  auto* desks_controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(InOverviewSession());

  // Create a new desk (we have two desks now).
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  EXPECT_EQ(2u, desks_controller->desks().size());

  // The current active desk is the first desk.
  EXPECT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Move highlight to the second desk.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DeskMiniView* mini_view_1 =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
          ->desks_bar_view()
          ->mini_views()[1];

  overview_session->highlight_controller()->MoveHighlightToView(mini_view_1);
  EXPECT_TRUE(mini_view_1->IsViewHighlighted());

  // Exit overview with 3-fingers downward swipes.
  DeskSwitchAnimationWaiter waiter;
  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  Scroll(0, -long_scroll, 3);
  waiter.Wait();
  EXPECT_FALSE(InOverviewSession());

  // Current active desk changes to the second desk.
  EXPECT_EQ(1, desks_controller->GetActiveDeskIndex());
}

class ReverseGestureHandlerTest : public WmGestureHandlerTest {
 public:
  ReverseGestureHandlerTest() = default;
  ReverseGestureHandlerTest(const ReverseGestureHandlerTest&) = delete;
  ReverseGestureHandlerTest& operator=(const ReverseGestureHandlerTest&) =
      delete;
  ~ReverseGestureHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kReverseScrollGestures);
    AshTestBase::SetUp();

    // Set natural scroll on.
    PrefService* pref =
        Shell::Get()->session_controller()->GetActivePrefService();
    pref->SetBoolean(prefs::kTouchpadEnabled, true);
    pref->SetBoolean(prefs::kNaturalScroll, true);
    pref->SetBoolean(prefs::kMouseReverseScroll, true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReverseGestureHandlerTest, Overview) {
  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;

  // Use the new gestures.
  // Swiping up with three fingers enters overview.
  Scroll(0, long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());

  // Swiping up again with three fingers does nothing.
  Scroll(0, long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());

  // Swiping down with three fingers exits overview.
  Scroll(0, -long_scroll, 3);
  EXPECT_FALSE(InOverviewSession());

  // Swiping down again with three fingers does nothing.
  Scroll(0, -long_scroll, 3);
  EXPECT_FALSE(InOverviewSession());
}

TEST_F(ReverseGestureHandlerTest, SwitchDesk) {
  // Add a new desk2.
  AddDesk();
  const Desk* desk1 = GetActiveDesk();
  const Desk* desk2 = GetNextDesk();

  // Scroll left to get next desk.
  ScrollToSwitchDesks(/*scroll_left=*/true);
  EXPECT_EQ(desk2, GetActiveDesk());
  // Scroll right to get previous desk.
  ScrollToSwitchDesks(/*scroll_left=*/false);
  EXPECT_EQ(desk1, GetActiveDesk());
}

}  // namespace ash
