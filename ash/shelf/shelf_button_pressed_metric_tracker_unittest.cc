// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_button_pressed_metric_tracker.h"

#include <utility>

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/button.h"

namespace ash {
namespace {

// A simple light weight test double dummy for a views::Button.
class DummyButton : public views::Button {
 public:
  DummyButton();

  DummyButton(const DummyButton&) = delete;
  DummyButton& operator=(const DummyButton&) = delete;
};

DummyButton::DummyButton() : views::Button(views::Button::PressedCallback()) {}

// Test fixture for the ShelfButtonPressedMetricTracker class. Relies on
// AshTestBase to initilize the UserMetricsRecorder and it's dependencies.
class ShelfButtonPressedMetricTrackerTest : public AshTestBase {
 public:
  ShelfButtonPressedMetricTrackerTest();

  ShelfButtonPressedMetricTrackerTest(
      const ShelfButtonPressedMetricTrackerTest&) = delete;
  ShelfButtonPressedMetricTrackerTest& operator=(
      const ShelfButtonPressedMetricTrackerTest&) = delete;

  ~ShelfButtonPressedMetricTrackerTest() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Calls ButtonPressed on the test target with the given |event|
  // and dummy values for the |sender| and |performed_action| parameters.
  void ButtonPressed(const ui::Event& event);

  // Calls ButtonPressed on the test target with the given |performed_action|
  // and dummy values for the |event| and |sender| parameters.
  void ButtonPressed(ShelfAction performed_action);

  // Calls ButtonPressed on the test target with the given |sender| and
  // |performed_action| and a dummy value for the |event| parameter.
  void ButtonPressed(const views::Button* sender, ShelfAction performed_action);

 protected:
  // The test target. Not owned.
  raw_ptr<ShelfButtonPressedMetricTracker> metric_tracker_;
};

ShelfButtonPressedMetricTrackerTest::ShelfButtonPressedMetricTrackerTest() =
    default;

ShelfButtonPressedMetricTrackerTest::~ShelfButtonPressedMetricTrackerTest() =
    default;

void ShelfButtonPressedMetricTrackerTest::SetUp() {
  AshTestBase::SetUp();

  Shelf* shelf = GetPrimaryShelf();
  ShelfViewTestAPI shelf_view_test_api(shelf->GetShelfViewForTesting());

  metric_tracker_ = shelf_view_test_api.shelf_button_pressed_metric_tracker();
}

void ShelfButtonPressedMetricTrackerTest::TearDown() {
  metric_tracker_ = nullptr;
  AshTestBase::TearDown();
}

void ShelfButtonPressedMetricTrackerTest::ButtonPressed(
    const ui::Event& event) {
  const DummyButton kDummyButton;
  metric_tracker_->ButtonPressed(event, &kDummyButton, SHELF_ACTION_NONE);
}

void ShelfButtonPressedMetricTrackerTest::ButtonPressed(
    ShelfAction performed_action) {
  const ui::test::TestEvent kDummyEvent(ui::EventType::kGestureTap);
  const DummyButton kDummyButton;
  metric_tracker_->ButtonPressed(kDummyEvent, &kDummyButton, performed_action);
}

void ShelfButtonPressedMetricTrackerTest::ButtonPressed(
    const views::Button* sender,
    ShelfAction performed_action) {
  const ui::test::TestEvent kDummyEvent(ui::EventType::kGestureTap);
  metric_tracker_->ButtonPressed(kDummyEvent, sender, performed_action);
}

}  // namespace

// Verifies that a Launcher_ButtonPressed_Mouse UMA user action is recorded when
// a button is pressed by a mouse event.
TEST_F(ShelfButtonPressedMetricTrackerTest,
       Launcher_ButtonPressed_MouseIsRecordedWhenIconActivatedByMouse) {
  const ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(),
                                   gfx::Point(), base::TimeTicks(), 0, 0);

  base::UserActionTester user_action_tester;
  ButtonPressed(mouse_event);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Launcher_ButtonPressed_Mouse"));
}

// Verifies that a Launcher_ButtonPressed_Touch UMA user action is recorded when
// a button is pressed by a touch event.
TEST_F(ShelfButtonPressedMetricTrackerTest,
       Launcher_ButtonPressed_TouchIsRecordedWhenIconActivatedByTouch) {
  const ui::TouchEvent touch_event(
      ui::EventType::kGestureTap, gfx::Point(), base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));

  base::UserActionTester user_action_tester;
  ButtonPressed(touch_event);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Launcher_ButtonPressed_Touch"));
}

// Verifies that a Launcher_LaunchTask UMA user action is recorded when
// pressing a button causes a new window to be created.
TEST_F(ShelfButtonPressedMetricTrackerTest,
       Launcher_LaunchTaskIsRecordedWhenNewWindowIsCreated) {
  base::UserActionTester user_action_tester;
  ButtonPressed(SHELF_ACTION_NEW_WINDOW_CREATED);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_LaunchTask"));
}

// Verifies that a Launcher_MinimizeTask UMA user action is recorded when
// pressing a button causes an existing window to be minimized.
TEST_F(ShelfButtonPressedMetricTrackerTest,
       Launcher_MinimizeTaskIsRecordedWhenWindowIsMinimized) {
  base::UserActionTester user_action_tester;
  ButtonPressed(SHELF_ACTION_WINDOW_MINIMIZED);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_MinimizeTask"));
}

// Verifies that a Launcher_SwitchTask UMA user action is recorded when
// pressing a button causes an existing window to be activated.
TEST_F(ShelfButtonPressedMetricTrackerTest,
       Launcher_SwitchTaskIsRecordedWhenExistingWindowIsActivated) {
  base::UserActionTester user_action_tester;
  ButtonPressed(SHELF_ACTION_WINDOW_ACTIVATED);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_SwitchTask"));
}

}  // namespace ash
