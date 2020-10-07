// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kNumFingersForWindowCycle = 2;
constexpr int kNumFingersForHighlight = 3;
constexpr int kNumFingersForDesksSwitch = 4;

constexpr char kOverviewGestureNotificationId[] =
    "ash.wm.reverse_overview_gesture";

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

bool IsOverviewReverseGestureNotificationShown() {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
      kOverviewGestureNotificationId);
}

void CloseOverviewReverseGestureNotification() {
  if (IsOverviewReverseGestureNotificationShown()) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kOverviewGestureNotificationId, true);
  }
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
  ~WmGestureHandlerTest() override = default;

  void Scroll(float x_offset, float y_offset, int fingers) {
    GetEventGenerator()->ScrollSequence(
        gfx::Point(), base::TimeDelta::FromMilliseconds(5),
        GetOffsetX(x_offset), GetOffsetY(y_offset), /*steps=*/100, fingers);
  }

  void ScrollToSwitchDesks(bool scroll_left) {
    DeskSwitchAnimationWaiter waiter;
    const float x_offset =
        (scroll_left ? -1 : 1) * WmGestureHandler::kHorizontalThresholdDp;
    Scroll(x_offset, 0, kNumFingersForDesksSwitch);
    waiter.Wait();
  }

  void MouseWheelScroll(int delta_x, int delta_y, int num_of_times) {
    auto* generator = GetEventGenerator();
    for (int i = 0; i < num_of_times; i++)
      generator->MoveMouseWheel(delta_x, delta_y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WmGestureHandlerTest);
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

  // Swiping down can enter overview but a notification will be shown.
  Scroll(0, -long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());
  // Notification is shown for the first time.
  EXPECT_TRUE(IsOverviewReverseGestureNotificationShown());
  CloseOverviewReverseGestureNotification();

  // Swiping up can exit overview, but a notification will be shown.
  Scroll(0, long_scroll, 3);
  EXPECT_FALSE(InOverviewSession());
  // Notification is shown for the second time.
  EXPECT_TRUE(IsOverviewReverseGestureNotificationShown());
  CloseOverviewReverseGestureNotification();

  // Swiping down triggers the notification for the last time.
  Scroll(0, -long_scroll, 3);
  EXPECT_TRUE(InOverviewSession());
  // Notification is shown for the third (last) time.
  EXPECT_TRUE(IsOverviewReverseGestureNotificationShown());
  CloseOverviewReverseGestureNotification();

  // Doing wrong gesture again won't trigger notification anymore.
  Scroll(0, long_scroll, 3);
  EXPECT_FALSE(InOverviewSession());
  // No notification will be shown anymore.
  EXPECT_FALSE(IsOverviewReverseGestureNotificationShown());
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
  Shell::Get()->overview_controller()->StartOverview();
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

// The tests that verifies Virtual Desks gestures will be parameterized
// separately to run only when Virtual Desks and its gestures are enabled.
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

// Tests that a large scroll only moves to the next desk.
TEST_F(DesksGestureHandlerTest, NoDoubleDeskChange) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(4u, desk_controller->desks().size());
  ASSERT_EQ(desk_controller->desks()[0].get(), desk_controller->active_desk());

  const float long_scroll = WmGestureHandler::kHorizontalThresholdDp * 3;
  DeskSwitchAnimationWaiter waiter;
  Scroll(long_scroll, 0, kNumFingersForDesksSwitch);
  waiter.Wait();
  EXPECT_EQ(desk_controller->desks()[1].get(), desk_controller->active_desk());
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

class InteractiveWindowCycleListGestureHandlerTest
    : public WmGestureHandlerTest {
 public:
  InteractiveWindowCycleListGestureHandlerTest() = default;
  InteractiveWindowCycleListGestureHandlerTest(
      const InteractiveWindowCycleListGestureHandlerTest&) = delete;
  InteractiveWindowCycleListGestureHandlerTest& operator=(
      const InteractiveWindowCycleListGestureHandlerTest&) = delete;
  ~InteractiveWindowCycleListGestureHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kInteractiveWindowCycleList);
    AshTestBase::SetUp();
    WindowCycleList::DisableInitialDelayForTesting();
  }

  int GetCurrentIndex() const {
    return Shell::Get()
        ->window_cycle_controller()
        ->window_cycle_list()
        ->current_index_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests three finger horizontal scroll gesture to move selection left or right.
TEST_F(InteractiveWindowCycleListGestureHandlerTest,
       ThreeFingerHorizontalScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    Scroll(GetOffsetX(x_offset), GetOffsetY(y_offset), kNumFingersForHighlight);
    controller->CompleteCycling();
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(-horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Open an overview session and window cycle list. Scroll right to second
  // item. Scroll should only go to the window cycle list.
  // Current order is [2,4,5,3,1].
  Shell::Get()->overview_controller()->StartOverview();
  EXPECT_TRUE(InOverviewSession());

  Shell::Get()->window_cycle_controller()->StartCycling();
  Scroll(GetOffsetX(horizontal_scroll), 0, kNumFingersForHighlight);
  EXPECT_EQ(nullptr, GetHighlightedWindow());

  Shell::Get()->window_cycle_controller()->CompleteCycling();
  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));
}

// Tests two finger horizontal scroll gesture to move selection left or right.
TEST_F(InteractiveWindowCycleListGestureHandlerTest,
       TwoFingerHorizontalScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    // Since two finger swipes are negated, negate in tests to mimic how this
    // actually behaves on devices.
    Scroll(GetOffsetX(-x_offset), GetOffsetY(y_offset),
           kNumFingersForWindowCycle);
    controller->CompleteCycling();
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(-horizontal_scroll * 3, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests mouse wheel scroll gesture to move selection left or right.
TEST_F(InteractiveWindowCycleListGestureHandlerTest,
       MouseWheelScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset,
                                                            int num_of_times) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    MouseWheelScroll(x_offset, y_offset, num_of_times);
    controller->CompleteCycling();
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that swiping up closes window cycle if it's open and starts overview
// mode.
// TODO(chinsenj): Add this test to
// WmGestureHandlerTest.VerticalScrolls after this feature is launched.
TEST_F(InteractiveWindowCycleListGestureHandlerTest, VerticalScroll) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  const float vertical_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;
  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();

  // Start cycling and then swipe up to open up overview.
  window_cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  Scroll(0, vertical_scroll, 3);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_FALSE(window_cycle_controller->IsCycling());

  // Start cycling and then swipe down.
  window_cycle_controller->HandleCycleWindow(WindowCycleController::FORWARD);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  Scroll(0, -vertical_scroll, 3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());

  // Swipe diagonally with horizontal bias.
  Scroll(horizontal_scroll * 3, vertical_scroll, 3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  EXPECT_FALSE(InOverviewSession());

  // Swipe diagonally with vertical bias.
  Scroll(horizontal_scroll, vertical_scroll, 3);
  EXPECT_FALSE(window_cycle_controller->IsCycling());
  EXPECT_TRUE(InOverviewSession());
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
    scoped_feature_list_.InitWithFeatures(
        {features::kInteractiveWindowCycleList,
         features::kReverseScrollGestures},
        {});
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
  ScrollToSwitchDesks(/*scroll_right=*/false);
  EXPECT_EQ(desk1, GetActiveDesk());
}

// Tests mouse wheel scroll gesture to move selection left or right. Mouse
// reverse scroll should reverse its direction.
TEST_F(ReverseGestureHandlerTest, MouseWheelScrollInWindowCycleList) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset,
                                                            int num_of_times) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    MouseWheelScroll(x_offset, y_offset, num_of_times);
    controller->CompleteCycling();
  };

  // Start cycle, simulating alt key being held down. Scroll right to fourth
  // item.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Start cycle. Scroll left to third item.
  // Current order is [2,5,4,3,1].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 3);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle. Scroll right to second item.
  // Current order is [4,2,5,3,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Turn mouse reverse scroll off.
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref->SetBoolean(prefs::kMouseReverseScroll, false);

  // Start cycle. Scroll left once.
  // Current order is [2,4,5,3,1].
  scroll_until_window_highlighted_and_confirm(0, horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Start cycle. Scroll right once.
  // Current order is [1,2,4,5,3].
  scroll_until_window_highlighted_and_confirm(0, -horizontal_scroll, 1);
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that natural scroll doesn't affect two and three finger horizontal
// scroll gestures for cycling window cycle list.
TEST_F(ReverseGestureHandlerTest, WindowCycleListTrackpadGestures) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds);
  const float horizontal_scroll = WmGestureHandler::kHorizontalThresholdDp;

  auto scroll_until_window_highlighted_and_confirm = [this](float x_offset,
                                                            float y_offset,
                                                            int num_fingers) {
    WindowCycleController* controller = Shell::Get()->window_cycle_controller();
    controller->StartCycling();
    Scroll(x_offset, y_offset, num_fingers);
    controller->CompleteCycling();
  };

  // Start cycle, scroll right with two finger gesture.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0,
                                              kNumFingersForWindowCycle);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle, scroll right with three finger gesture.
  // Current order is [4,5,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0,
                                              kNumFingersForHighlight);
  EXPECT_TRUE(wm::IsActiveWindow(window5.get()));

  // Turn natural scroll off.
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref->SetBoolean(prefs::kNaturalScroll, false);

  // Start cycle, scroll right with two finger gesture. Note: two figner swipes
  // are negated, so negate in tests to mimic how this actually behaves on
  // devices.
  // Current order is [5,4,3,2,1].
  scroll_until_window_highlighted_and_confirm(-horizontal_scroll, 0,
                                              kNumFingersForWindowCycle);
  EXPECT_TRUE(wm::IsActiveWindow(window4.get()));

  // Start cycle, scroll right with three finger gesture.
  // Current order is [4,5,3,2,1].
  scroll_until_window_highlighted_and_confirm(horizontal_scroll, 0,
                                              kNumFingersForHighlight);
  EXPECT_TRUE(wm::IsActiveWindow(window5.get()));
}

}  // namespace ash
