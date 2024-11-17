// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_touchpad_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/accessibility/test_event_recorder.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

const int kUsbMouseDeviceId = 20;
const int kBluetoothMouseDeviceId = 25;
const int kInternalTouchpadDeviceId = 30;
const uint16_t kLogitechVID = 0x046d;
const uint16_t kMousePID = 0xb034;

ui::InputDevice GetSampleMouseUsb() {
  return {kUsbMouseDeviceId, ui::INPUT_DEVICE_USB, "SampleMouseUsb"};
}

ui::InputDevice GetSampleMouseBluetooth() {
  return {kBluetoothMouseDeviceId,
          ui::INPUT_DEVICE_BLUETOOTH,
          "SampleMouseBluetooth",
          /* phys= */ "",
          base::FilePath(),
          kLogitechVID,
          kMousePID,
          /* version= */ 0};
}

ui::TouchpadDevice GetSampleTouchpadInternal() {
  return {kInternalTouchpadDeviceId, ui::INPUT_DEVICE_INTERNAL, "touchpad"};
}

void SimulateOnlyInternalTouchpadConnected() {
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {GetSampleTouchpadInternal()});
}

void SimulateExternalMouseConnected() {
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {GetSampleMouseUsb(), GetSampleMouseBluetooth()});

  SimulateOnlyInternalTouchpadConnected();
}

void SetDisableTouchpadMode(DisableTouchpadMode mode) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetInteger(prefs::kAccessibilityDisableTrackpadMode,
                    static_cast<int>(mode));
}

}  // namespace

class DisableTouchpadEventRewriterTest : public AshTestBase {
 public:
  DisableTouchpadEventRewriterTest() = default;
  DisableTouchpadEventRewriterTest(const DisableTouchpadEventRewriterTest&) =
      delete;
  DisableTouchpadEventRewriterTest& operator=(
      const DisableTouchpadEventRewriterTest&) = delete;
  ~DisableTouchpadEventRewriterTest() override = default;

  void PressAndReleaseShiftKey() {
    generator()->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
    generator()->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    event_rewriter_ = std::make_unique<DisableTouchpadEventRewriter>();
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        event_rewriter());
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
    event_rewriter()->SetEnabled(true);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        event_rewriter());
    event_rewriter_.reset();
    generator_ = nullptr;
    AshTestBase::TearDown();
  }

  ui::test::EventGenerator* generator() { return generator_; }
  TestEventRecorder* event_recorder() { return &event_recorder_; }
  DisableTouchpadEventRewriter* event_rewriter() {
    return event_rewriter_.get();
  }

 private:
  // Generates ui::Events to simulate user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // DisableTouchpadEventRewriter.
  TestEventRecorder event_recorder_;
  // The DisableTouchpadEventRewriter instance.
  std::unique_ptr<DisableTouchpadEventRewriter> event_rewriter_;
};

TEST_F(DisableTouchpadEventRewriterTest, KeyboardEventsNotCanceledIfDisabled) {
  event_rewriter()->SetEnabled(false);
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTouchpadEventRewriterTest, MouseButtonsNotCanceledIfDisabled) {
  event_rewriter()->SetEnabled(false);
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMousePressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());
  EXPECT_EQ(ui::EventType::kMouseReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTouchpadEventRewriterTest, KeyboardEventsNotCanceled) {
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTouchpadEventRewriterTest, MouseButtonsCanceledInAlwaysMode) {
  event_rewriter()->SetEnabled(true);
  SetDisableTouchpadMode(DisableTouchpadMode::kAlways);
  generator()->set_mouse_source_device_id(kInternalTouchpadDeviceId);
  SimulateOnlyInternalTouchpadConnected();

  generator()->PressLeftButton();
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorVisible());
  EXPECT_EQ(0U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(0U, event_recorder()->events().size());
}

TEST_F(DisableTouchpadEventRewriterTest, DisableAfterFiveShiftKeyPresses) {
  event_rewriter()->SetEnabled(true);

  int shiftKeyPressCount = 0;

  // Simulate pressing and releasing the shift key 5 times.
  for (int i = 0; i < 5; ++i) {
    PressAndReleaseShiftKey();
    generator()->AdvanceClock(base::Milliseconds(100));
    ++shiftKeyPressCount;

    // After the 5th press, check if the rewriter is disabled.
    if (shiftKeyPressCount == 5) {
      EXPECT_FALSE(event_rewriter()->IsEnabled());
    } else {
      EXPECT_TRUE(event_rewriter()->IsEnabled());
    }
  }
}

TEST_F(DisableTouchpadEventRewriterTest,
       ShiftPressesExceedTimeWindowStaysEnabled) {
  event_rewriter()->SetEnabled(true);

  // Simulate pressing and releasing the shift key 4 times.
  for (int i = 0; i < 4; ++i) {
    PressAndReleaseShiftKey();
    generator()->AdvanceClock(base::Milliseconds(100));
    EXPECT_TRUE(event_rewriter()->IsEnabled());
  }

  // Exceed shift key time window on final key press.
  generator()->AdvanceClock(base::Seconds(3));
  PressAndReleaseShiftKey();
  EXPECT_TRUE(event_rewriter()->IsEnabled());
}

TEST_F(DisableTouchpadEventRewriterTest, ResetShiftKeyCountOnNonShiftKeyPress) {
  event_rewriter()->SetEnabled(true);

  // Simulate pressing shift key and releasing the shift key 3 times.
  for (int i = 0; i < 3; ++i) {
    PressAndReleaseShiftKey();
    EXPECT_TRUE(event_rewriter()->IsEnabled());
  }

  // Press non shift key resets the shift key  count.
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  EXPECT_TRUE(event_rewriter()->IsEnabled());

  int shiftKeyPressCount = 0;

  for (int i = 0; i < 5; ++i) {
    PressAndReleaseShiftKey();
    generator()->AdvanceClock(base::Milliseconds(100));
    ++shiftKeyPressCount;

    if (shiftKeyPressCount == 5) {
      EXPECT_FALSE(event_rewriter()->IsEnabled());
    } else {
      EXPECT_TRUE(event_rewriter()->IsEnabled());
    }
  }
}

TEST_F(DisableTouchpadEventRewriterTest,
       InternalMouseCanceledWithExternalMouse) {
  event_rewriter()->SetEnabled(true);
  SetDisableTouchpadMode(DisableTouchpadMode::kOnExternalMouseConnected);
  generator()->set_mouse_source_device_id(kInternalTouchpadDeviceId);

  SimulateOnlyInternalTouchpadConnected();
  generator()->PressLeftButton();
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorVisible());
  EXPECT_EQ(1U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());

  SimulateExternalMouseConnected();
  generator()->PressLeftButton();
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorVisible());
  EXPECT_EQ(2U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());
}

TEST_F(DisableTouchpadEventRewriterTest, ExternalMouseAllowedWhenConnected) {
  event_rewriter()->SetEnabled(true);
  SetDisableTouchpadMode(DisableTouchpadMode::kOnExternalMouseConnected);
  generator()->set_mouse_source_device_id(kInternalTouchpadDeviceId);

  SimulateOnlyInternalTouchpadConnected();
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorVisible());
  generator()->PressLeftButton();
  EXPECT_EQ(1U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(2U, event_recorder()->events().size());

  SimulateExternalMouseConnected();
  generator()->set_mouse_source_device_id(kUsbMouseDeviceId);
  generator()->PressLeftButton();
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorVisible());
  EXPECT_EQ(3U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(4U, event_recorder()->events().size());
}

}  // namespace ash
