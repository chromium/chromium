// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_trackpad_event_rewriter.h"

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
const int kInternalTrackpadDeviceId = 30;
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

ui::TouchpadDevice GetSampleTrackpadInternal() {
  return {kInternalTrackpadDeviceId, ui::INPUT_DEVICE_INTERNAL, "touchpad"};
}

void SimulateOnlyInternalTrackpadConnected() {
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {GetSampleTrackpadInternal()});
}

void SimulateExternalMouseConnected() {
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {GetSampleMouseUsb(), GetSampleMouseBluetooth()});

  SimulateOnlyInternalTrackpadConnected();
}

void SetDisableTrackpadMode(DisableTrackpadMode mode) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetInteger(prefs::kAccessibilityDisableTrackpadMode,
                    static_cast<int>(mode));
}

}  // namespace

class DisableTrackpadEventRewriterTest : public AshTestBase {
 public:
  DisableTrackpadEventRewriterTest() = default;
  DisableTrackpadEventRewriterTest(const DisableTrackpadEventRewriterTest&) =
      delete;
  DisableTrackpadEventRewriterTest& operator=(
      const DisableTrackpadEventRewriterTest&) = delete;
  ~DisableTrackpadEventRewriterTest() override = default;

  void PressAndReleaseEscapeKey() {
    generator()->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
    generator()->ReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    event_rewriter_ = std::make_unique<DisableTrackpadEventRewriter>();
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
  DisableTrackpadEventRewriter* event_rewriter() {
    return event_rewriter_.get();
  }

 private:
  // Generates ui::Events to simulate user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  // Records events delivered to the next event rewriter after
  // DisableTrackpadEventRewriter.
  TestEventRecorder event_recorder_;
  // The DisableTrackpadEventRewriter instance.
  std::unique_ptr<DisableTrackpadEventRewriter> event_rewriter_;
};

TEST_F(DisableTrackpadEventRewriterTest, KeyboardEventsNotCanceledIfDisabled) {
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

TEST_F(DisableTrackpadEventRewriterTest, MouseButtonsNotCanceledIfDisabled) {
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

TEST_F(DisableTrackpadEventRewriterTest, KeyboardEventsNotCanceled) {
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1U, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyPressed,
            event_recorder()->events().back()->type());
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(2u, event_recorder()->events().size());
  ASSERT_EQ(ui::EventType::kKeyReleased,
            event_recorder()->events().back()->type());
}

TEST_F(DisableTrackpadEventRewriterTest, MouseButtonsCanceledInAlwaysMode) {
  event_rewriter()->SetEnabled(true);
  SetDisableTrackpadMode(DisableTrackpadMode::kAlways);
  generator()->set_mouse_source_device_id(kInternalTrackpadDeviceId);
  SimulateOnlyInternalTrackpadConnected();

  generator()->PressLeftButton();
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorVisible());
  EXPECT_EQ(0U, event_recorder()->events().size());
  generator()->ReleaseLeftButton();
  EXPECT_EQ(0U, event_recorder()->events().size());
}

TEST_F(DisableTrackpadEventRewriterTest, DisableAfterFiveEscapeKeyPresses) {
  event_rewriter()->SetEnabled(true);

  int escapeKeyPressCount = 0;

  // Simulate pressing and releasing the escape key 5 times.
  for (int i = 0; i < 5; ++i) {
    PressAndReleaseEscapeKey();
    generator()->AdvanceClock(base::Milliseconds(500));
    ++escapeKeyPressCount;

    // After the 5th press, check if the rewriter is disabled.
    if (escapeKeyPressCount == 5) {
      EXPECT_FALSE(event_rewriter()->IsEnabled());
    } else {
      EXPECT_TRUE(event_rewriter()->IsEnabled());
    }
  }
}

TEST_F(DisableTrackpadEventRewriterTest,
       EscapePressesExceedTimeWindowStaysEnabled) {
  event_rewriter()->SetEnabled(true);

  // Simulate pressing and releasing the escape key 4 times.
  for (int i = 0; i < 4; ++i) {
    PressAndReleaseEscapeKey();
    generator()->AdvanceClock(base::Milliseconds(500));
    EXPECT_TRUE(event_rewriter()->IsEnabled());
  }

  // Exceed escape key time window on final key press.
  generator()->AdvanceClock(base::Seconds(2));
  PressAndReleaseEscapeKey();
  EXPECT_TRUE(event_rewriter()->IsEnabled());
}

TEST_F(DisableTrackpadEventRewriterTest,
       ResetEscapeKeyCountOnNonEscapeKeyPress) {
  event_rewriter()->SetEnabled(true);

  // Simulate pressing escape key and releasing the escape key 3 times.
  for (int i = 0; i < 3; ++i) {
    PressAndReleaseEscapeKey();
    EXPECT_TRUE(event_rewriter()->IsEnabled());
  }

  // Press non escape key resets the escape key  count.
  generator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  generator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  EXPECT_TRUE(event_rewriter()->IsEnabled());

  int escapeKeyPressCount = 0;

  for (int i = 0; i < 5; ++i) {
    PressAndReleaseEscapeKey();
    generator()->AdvanceClock(base::Milliseconds(500));
    ++escapeKeyPressCount;

    if (escapeKeyPressCount == 5) {
      EXPECT_FALSE(event_rewriter()->IsEnabled());
    } else {
      EXPECT_TRUE(event_rewriter()->IsEnabled());
    }
  }
}

TEST_F(DisableTrackpadEventRewriterTest,
       InternalMouseCanceledWithExternalMouse) {
  event_rewriter()->SetEnabled(true);
  SetDisableTrackpadMode(DisableTrackpadMode::kOnExternalMouseConnected);
  generator()->set_mouse_source_device_id(kInternalTrackpadDeviceId);

  SimulateOnlyInternalTrackpadConnected();
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

TEST_F(DisableTrackpadEventRewriterTest, ExternalMouseAllowedWhenConnected) {
  event_rewriter()->SetEnabled(true);
  SetDisableTrackpadMode(DisableTrackpadMode::kOnExternalMouseConnected);
  generator()->set_mouse_source_device_id(kInternalTrackpadDeviceId);

  SimulateOnlyInternalTrackpadConnected();
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
