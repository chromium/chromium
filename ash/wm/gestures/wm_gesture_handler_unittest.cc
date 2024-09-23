// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/wm_gesture_handler.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/test/mock_input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/capture_controller.h"

namespace ash {

namespace {

constexpr InputDeviceSettingsController::DeviceId kTouchpadId =
    ui::ED_UNKNOWN_DEVICE;

bool InOverviewSession() {
  return Shell::Get()->overview_controller()->InOverviewSession();
}

const aura::Window* GetFocusedWindow() {
  if (!InOverviewSession()) {
    return nullptr;
  }

  views::View* focused_view = GetFocusedView();
  return views::IsViewClass<OverviewItemView>(focused_view)
             ? focused_view->GetWidget()->GetNativeWindow()
             : nullptr;
}

class TestInputDeviceSettingsController
    : public MockInputDeviceSettingsController {
 public:
  TestInputDeviceSettingsController()
      : touchpad_settings_(
            /*sensitivity=*/kDefaultSensitivity,
            kDefaultReverseScrolling,
            kDefaultAccelerationEnabled,
            kDefaultTapToClickEnabled,
            kDefaultThreeFingerClickEnabled,
            kDefaultTapDraggingEnabled,
            /*scroll_sensitivity=*/kDefaultScrollSensitivity,
            kDefaultScrollAccelerationEnabled,
            kDefaultHapticSensitivity,
            kDefaultHapticFeedbackEnabled,
            /*simulate_right_click=*/
            ui::mojom::SimulateRightClickModifier::kNone) {}
  TestInputDeviceSettingsController(const TestInputDeviceSettingsController&) =
      delete;
  TestInputDeviceSettingsController& operator=(
      const TestInputDeviceSettingsController*) = delete;
  ~TestInputDeviceSettingsController() override = default;

  void SetTouchpadReverseScrollingEnabled(bool enable_reverse_scrolling) {
    touchpad_settings_.reverse_scrolling = enable_reverse_scrolling;
  }

  // MockInputDeviceSettingsController:
  const mojom::TouchpadSettings* GetTouchpadSettings(DeviceId id) override {
    return &touchpad_settings_;
  }

 private:
  mojom::TouchpadSettings touchpad_settings_;
};

}  // namespace

class WmGestureHandlerTest : public AshTestBase {
 public:
  WmGestureHandlerTest() = default;
  WmGestureHandlerTest(const WmGestureHandlerTest&) = delete;
  WmGestureHandlerTest& operator=(const WmGestureHandlerTest&) = delete;
  ~WmGestureHandlerTest() override = default;

  int GetOffsetY(int y_offset) {
    return input_controller_->GetTouchpadSettings(kTouchpadId)
                   ->reverse_scrolling
               ? -y_offset
               : y_offset;
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Reset existing input device settings controller.
    scoped_input_controller_resetter_ = std::make_unique<
        InputDeviceSettingsController::ScopedResetterForTest>();
    // Create a test input device settings controller for test use.
    input_controller_ = std::make_unique<TestInputDeviceSettingsController>();
    // Disable touchpad reverse scrolling.
    SetTouchpadReverseScrollingEnabled(false);
  }

  void TearDown() override {
    // Reset test input controller and the controller resetter.
    input_controller_.reset();
    scoped_input_controller_resetter_.reset();

    AshTestBase::TearDown();
  }

  void Scroll(float x_offset, float y_offset, int fingers) {
    GetEventGenerator()->ScrollSequence(gfx::Point(), base::Milliseconds(5),
                                        x_offset, GetOffsetY(y_offset),
                                        /*steps=*/100, fingers);
  }

  void MouseWheelScroll(int delta_x, int delta_y, int num_of_times) {
    auto* generator = GetEventGenerator();
    for (int i = 0; i < num_of_times; i++)
      generator->MoveMouseWheel(delta_x, delta_y);
  }

 protected:
  void SetTouchpadReverseScrollingEnabled(bool enable_reverse_scrolling) {
    input_controller_->SetTouchpadReverseScrollingEnabled(
        enable_reverse_scrolling);
  }

 private:
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      scoped_input_controller_resetter_;
  std::unique_ptr<TestInputDeviceSettingsController> input_controller_;
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

  // Scrolls until a window is focused, ignoring any desks items (if any).
  auto scroll_until_window_focused = [this](float x_offset, float y_offset) {
    do {
      Scroll(x_offset, GetOffsetY(y_offset), kNumFingersForFocus);
    } while (!GetFocusedWindow());
  };

  // Select the first window first.
  scroll_until_window_focused(horizontal_scroll, 0);

  // Long scroll right moves selection to the fourth window.
  scroll_until_window_focused(horizontal_scroll * 3, 0);
  EXPECT_TRUE(InOverviewSession());

  // Short scroll left moves selection to the third window.
  scroll_until_window_focused(-horizontal_scroll, 0);
  EXPECT_TRUE(InOverviewSession());

  // Short scroll left moves selection to the second window.
  scroll_until_window_focused(-horizontal_scroll, 0);
  EXPECT_TRUE(InOverviewSession());

  // Swiping down (3 fingers) exits and selects the currently focused window.
  Scroll(0, -vertical_scroll, 3);
  EXPECT_FALSE(InOverviewSession());

  // Second MRU window is selected (i.e. |window4|).
  EXPECT_EQ(window4.get(), window_util::GetActiveWindow());
}

// Tests that a mostly horizontal scroll does not trigger overview.
TEST_F(WmGestureHandlerTest, HorizontalScrolls) {
  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  Scroll(long_scroll + 100, long_scroll, kNumFingersForFocus);
  EXPECT_FALSE(InOverviewSession());

  Scroll(-long_scroll - 100, long_scroll, kNumFingersForFocus);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that we only enter overview after a scroll has ended.
TEST_F(WmGestureHandlerTest, EnterOverviewOnScrollEnd) {
  base::TimeTicks timestamp = base::TimeTicks::Now();
  const int num_fingers = 3;
  base::TimeDelta step_delay(base::Milliseconds(5));
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel, gfx::Point(),
                               timestamp, 0, 0, 0, 0, 0, num_fingers);
  GetEventGenerator()->Dispatch(&fling_cancel);

  // Scroll up by 1000px. We are not in overview yet, because the scroll is
  // still ongoing.
  for (int i = 0; i < 100; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::EventType::kScroll, gfx::Point(), timestamp, 0, 0,
                         GetOffsetY(10), 0, GetOffsetY(10), num_fingers);
    GetEventGenerator()->Dispatch(&move);
  }
  ASSERT_FALSE(InOverviewSession());

  timestamp += step_delay;
  ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart, gfx::Point(),
                              timestamp, 0, 0, GetOffsetY(-10), 0,
                              GetOffsetY(-10), num_fingers);
  GetEventGenerator()->Dispatch(&fling_start);
  EXPECT_TRUE(InOverviewSession());
}

TEST_F(WmGestureHandlerTest, EnterOverviewWithNormalCaptureWindow) {
  base::TimeTicks timestamp = base::TimeTicks::Now();
  constexpr int num_fingers = 3;
  constexpr base::TimeDelta step_delay(base::Milliseconds(5));

  // If 3 finger scroll event while there is a capture window is set to the
  // normal type window, we should not handle the event as entering overview
  // mode.
  std::unique_ptr<aura::Window> normal_window =
      CreateTestWindow(gfx::Rect(100, 100));
  normal_window->SetCapture();

  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel, gfx::Point(),
                               timestamp, 0, 0, 0, 0, 0, num_fingers);
  GetEventGenerator()->Dispatch(&fling_cancel);

  // Send EventType::kScroll events to initializae ScrollData.
  for (int i = 0; i < 100; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::EventType::kScroll, gfx::Point(), timestamp, 0, 0,
                         GetOffsetY(10), 0, GetOffsetY(10), num_fingers);
    GetEventGenerator()->Dispatch(&move);
  }

  timestamp += step_delay;

  ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart, gfx::Point(),
                              timestamp, 0, 0, GetOffsetY(-10), 0,
                              GetOffsetY(-10), num_fingers);
  GetEventGenerator()->Dispatch(&fling_start);
  EXPECT_FALSE(InOverviewSession());
  normal_window->ReleaseCapture();
}

TEST_F(WmGestureHandlerTest, EnterOverviewWithPopupCaptureWindow) {
  base::TimeTicks timestamp = base::TimeTicks::Now();
  constexpr int num_fingers = 3;
  constexpr base::TimeDelta step_delay(base::Milliseconds(5));

  // If 3 finger scroll event while there is a capture window is set to the
  // window by not normal, we should ignore the capture state and handle the
  // event as entering overview mode.
  std::unique_ptr<aura::Window> normal_window =
      CreateTestWindow(gfx::Rect(100, 100));
  std::unique_ptr<aura::Window> popup_window =
      base::WrapUnique(aura::test::CreateTestWindowWithDelegateAndType(
          aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
          aura::client::WINDOW_TYPE_POPUP, /*id=*/1, gfx::Rect(100, 100),
          normal_window.get(), /*show_on_creation=*/true));
  popup_window->SetCapture();

  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel, gfx::Point(),
                               timestamp, 0, 0, 0, 0, 0, num_fingers);
  GetEventGenerator()->Dispatch(&fling_cancel);

  // Send EventType::kScroll events to initializae ScrollData.
  for (int i = 0; i < 100; ++i) {
    timestamp += step_delay;
    ui::ScrollEvent move(ui::EventType::kScroll, gfx::Point(), timestamp, 0, 0,
                         GetOffsetY(10), 0, GetOffsetY(10), num_fingers);
    GetEventGenerator()->Dispatch(&move);
  }

  timestamp += step_delay;

  ui::ScrollEvent fling_start(ui::EventType::kScrollFlingStart, gfx::Point(),
                              timestamp, 0, 0, GetOffsetY(-10), 0,
                              GetOffsetY(-10), num_fingers);
  GetEventGenerator()->Dispatch(&fling_start);
  EXPECT_TRUE(InOverviewSession());
  popup_window->ReleaseCapture();
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
  ScrollToSwitchDesks(/*scroll_left=*/false, GetEventGenerator());
  EXPECT_EQ(desk_controller->desks()[1].get(), desk_controller->active_desk());

  // Tests that scrolling left should take us to the previous desk.
  ScrollToSwitchDesks(/*scroll_left=*/true, GetEventGenerator());
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

// Tests that we activate focused desk when using 3-finger swipes to exit
// overview.
TEST_F(WmGestureHandlerTest, ActivateFocusedDeskWithVerticalScroll) {
  auto* desks_controller = DesksController::Get();

  EnterOverview();
  EXPECT_TRUE(InOverviewSession());

  // Create a new desk (we have two desks now).
  desks_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  EXPECT_EQ(2u, desks_controller->desks().size());

  // The current active desk is the first desk.
  EXPECT_EQ(0, desks_controller->GetActiveDeskIndex());

  // Move focus to the second desk.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DeskMiniView* mini_view_1 =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
          ->desks_bar_view()
          ->mini_views()[1];
  mini_view_1->desk_preview()->RequestFocus();

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
    WmGestureHandlerTest::SetUp();

    // Enable touchpad reverse scrolling.
    SetTouchpadReverseScrollingEnabled(true);
  }
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
  NewDesk();
  const Desk* desk1 = GetActiveDesk();
  const Desk* desk2 = GetNextDesk();

  // Scroll left to get next desk.
  ScrollToSwitchDesks(/*scroll_left=*/true, GetEventGenerator());
  EXPECT_EQ(desk2, GetActiveDesk());
  // Scroll right to get previous desk.
  ScrollToSwitchDesks(/*scroll_left=*/false, GetEventGenerator());
  EXPECT_EQ(desk1, GetActiveDesk());
}

// Test state for gestures in kiosk.
class WmGestureHandlerKioskTest : public WmGestureHandlerTest {
 public:
  WmGestureHandlerKioskTest() = default;
  WmGestureHandlerKioskTest(const WmGestureHandlerKioskTest&) = delete;
  WmGestureHandlerKioskTest& operator=(const WmGestureHandlerKioskTest&) =
      delete;
  ~WmGestureHandlerKioskTest() override = default;

  void SetUp() override {
    WmGestureHandlerTest::SetUp();
    SimulateKioskMode(user_manager::UserType::kWebKioskApp);
  }
};

// Tests that a three fingers upwards scroll gesture does not enter overview.
TEST_F(WmGestureHandlerKioskTest, VerticalScrollDisabledKiosk) {
  EXPECT_FALSE(InOverviewSession());

  const float long_scroll = 2 * WmGestureHandler::kVerticalThresholdDp;
  const int finger_count = 3;
  Scroll(0, long_scroll, finger_count);

  // Overview was not opened by gesture.
  EXPECT_FALSE(InOverviewSession());
}

}  // namespace ash
