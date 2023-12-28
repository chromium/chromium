// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/virtual_keyboard_controller.h"

#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/internal_input_devices_event_blocker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"

using keyboard::KeyboardEnableFlag;

namespace ash {

namespace {

VirtualKeyboardController* GetVirtualKeyboardController() {
  return Shell::Get()->keyboard_controller()->virtual_keyboard_controller();
}

}  // namespace

class VirtualKeyboardControllerTest : public AshTestBase {
 public:
  VirtualKeyboardControllerTest() = default;

  VirtualKeyboardControllerTest(const VirtualKeyboardControllerTest&) = delete;
  VirtualKeyboardControllerTest& operator=(
      const VirtualKeyboardControllerTest&) = delete;

  ~VirtualKeyboardControllerTest() override = default;

  display::Display GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay();
  }

  display::Display GetSecondaryDisplay() {
    return display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .GetSecondaryDisplay();
  }

  keyboard::KeyboardUIController* keyboard_ui_controller() {
    return keyboard::KeyboardUIController::Get();
  }
};

// Mock event blocker that enables the internal keyboard when it's destructor
// is called.
class MockEventBlocker : public InternalInputDevicesEventBlocker {
 public:
  MockEventBlocker() = default;

  MockEventBlocker(const MockEventBlocker&) = delete;
  MockEventBlocker& operator=(const MockEventBlocker&) = delete;

  ~MockEventBlocker() override {
    std::vector<ui::KeyboardDevice> keyboard_devices;
    keyboard_devices.push_back(ui::KeyboardDevice(
        1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  }
};

// Tests that reenabling keyboard devices while shutting down does not
// cause the Virtual Keyboard Controller to crash. See crbug.com/446204.
TEST_F(VirtualKeyboardControllerTest, RestoreKeyboardDevices) {
  // Toggle tablet mode on.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<InternalInputDevicesEventBlocker> blocker(
      new MockEventBlocker);
  TabletModeControllerTestApi().set_event_blocker(std::move(blocker));
}

TEST_F(VirtualKeyboardControllerTest,
       ForceToShowKeyboardWithKeysetWhenAccessibilityKeyboardIsEnabled) {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  accessibility_controller->virtual_keyboard().SetEnabled(true);
  ASSERT_TRUE(accessibility_controller->virtual_keyboard().enabled());

  // Set up a mock ImeControllerClient to test keyset changes.
  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(&client);

  // Should show the keyboard without messing with accessibility prefs.
  GetVirtualKeyboardController()->ForceShowKeyboardWithKeyset(
      input_method::ImeKeyset::kEmoji);
  EXPECT_TRUE(accessibility_controller->virtual_keyboard().enabled());

  // Keyset should be emoji.
  EXPECT_EQ(input_method::ImeKeyset::kEmoji, client.last_keyset_);

  // Simulate the keyboard hiding.
  if (keyboard_ui_controller()->HasObserver(GetVirtualKeyboardController())) {
    GetVirtualKeyboardController()->OnKeyboardHidden(
        false /* is_temporary_hide */);
  }
  base::RunLoop().RunUntilIdle();

  // The keyboard should still be enabled.
  EXPECT_TRUE(accessibility_controller->virtual_keyboard().enabled());

  // Reset the accessibility prefs.
  accessibility_controller->virtual_keyboard().SetEnabled(false);

  // Keyset should be reset to none.
  EXPECT_EQ(input_method::ImeKeyset::kNone, client.last_keyset_);

  Shell::Get()->ime_controller()->SetClient(nullptr);
}

TEST_F(VirtualKeyboardControllerTest,
       ForceToShowKeyboardWithKeysetWhenKeyboardIsDisabled) {
  // Set up a mock ImeControllerClient to test keyset changes.
  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(&client);

  // Should show the keyboard by enabling it temporarily.
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  EXPECT_FALSE(keyboard_ui_controller()->IsEnableFlagSet(
      KeyboardEnableFlag::kShelfEnabled));

  GetVirtualKeyboardController()->ForceShowKeyboardWithKeyset(
      input_method::ImeKeyset::kEmoji);

  EXPECT_TRUE(keyboard_ui_controller()->IsEnableFlagSet(
      KeyboardEnableFlag::kShelfEnabled));
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());

  // Keyset should be emoji.
  EXPECT_EQ(input_method::ImeKeyset::kEmoji, client.last_keyset_);

  // Simulate the keyboard hiding.
  if (keyboard_ui_controller()->HasObserver(GetVirtualKeyboardController())) {
    GetVirtualKeyboardController()->OnKeyboardHidden(
        false /* is_temporary_hide */);
  }
  base::RunLoop().RunUntilIdle();

  // The keyboard should still be disabled again.
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  EXPECT_FALSE(keyboard_ui_controller()->IsEnableFlagSet(
      KeyboardEnableFlag::kShelfEnabled));

  // Keyset should be reset to none.
  EXPECT_EQ(input_method::ImeKeyset::kNone, client.last_keyset_);
}

TEST_F(VirtualKeyboardControllerTest,
       ForceToShowKeyboardWithKeysetTemporaryHide) {
  // Set up a mock ImeControllerClient to test keyset changes.
  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(&client);

  // Should show the keyboard by enabling it temporarily.
  GetVirtualKeyboardController()->ForceShowKeyboardWithKeyset(
      input_method::ImeKeyset::kEmoji);

  EXPECT_TRUE(keyboard_ui_controller()->IsEnableFlagSet(
      KeyboardEnableFlag::kShelfEnabled));
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());

  // Keyset should be emoji.
  EXPECT_EQ(input_method::ImeKeyset::kEmoji, client.last_keyset_);

  // Simulate the keyboard hiding temporarily.
  if (keyboard_ui_controller()->HasObserver(GetVirtualKeyboardController())) {
    GetVirtualKeyboardController()->OnKeyboardHidden(
        true /* is_temporary_hide */);
  }
  base::RunLoop().RunUntilIdle();

  // The keyboard should still be enabled.
  EXPECT_TRUE(keyboard_ui_controller()->IsEnableFlagSet(
      KeyboardEnableFlag::kShelfEnabled));
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());

  // Keyset should still be emoji.
  EXPECT_EQ(input_method::ImeKeyset::kEmoji, client.last_keyset_);
}

class VirtualKeyboardControllerAutoTest : public VirtualKeyboardControllerTest,
                                          public VirtualKeyboardObserver {
 public:
  VirtualKeyboardControllerAutoTest() : notified_(false), suppressed_(false) {}

  VirtualKeyboardControllerAutoTest(const VirtualKeyboardControllerAutoTest&) =
      delete;
  VirtualKeyboardControllerAutoTest& operator=(
      const VirtualKeyboardControllerAutoTest&) = delete;

  ~VirtualKeyboardControllerAutoTest() override = default;

  void SetUp() override {
    VirtualKeyboardControllerTest::SetUp();
    Shell::Get()->system_tray_notifier()->AddVirtualKeyboardObserver(this);
  }

  void TearDown() override {
    Shell::Get()->system_tray_notifier()->RemoveVirtualKeyboardObserver(this);
    VirtualKeyboardControllerTest::TearDown();
  }

  void OnKeyboardSuppressionChanged(bool suppressed) override {
    notified_ = true;
    suppressed_ = suppressed;
  }

  void ResetObserver() {
    suppressed_ = false;
    notified_ = false;
  }

  bool IsVirtualKeyboardSuppressed() { return suppressed_; }

  bool notified() { return notified_; }

 private:
  // Whether the observer method was called.
  bool notified_;

  // Whether the keeyboard is suppressed.
  bool suppressed_;
};

// Tests that the onscreen keyboard is disabled if an internal keyboard is
// present and tablet mode is disabled.
TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfInternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  // Remove the internal keyboard. Virtual keyboard should now show.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  // Replug in the internal keyboard. Virtual keyboard should hide.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
}

TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfNoTouchScreen) {
  std::vector<ui::TouchscreenDevice> devices;
  // Add a touchscreen. Keyboard should deploy.
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB,
                            "Touchscreen", gfx::Size(800, 600), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(devices);
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  // Remove touchscreen. Keyboard should hide.
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
}

TEST_F(VirtualKeyboardControllerAutoTest, SuppressedIfExternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Touchscreen",
      gfx::Size(1024, 768), 0, false /* has_stylus */));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(
      ui::KeyboardDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_FALSE(IsVirtualKeyboardSuppressed());
}

// Tests handling multiple keyboards. Catches crbug.com/430252
TEST_F(VirtualKeyboardControllerAutoTest, HandleMultipleKeyboardsPresent) {
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  keyboards.push_back(
      ui::KeyboardDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  keyboards.push_back(
      ui::KeyboardDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboards);
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
}

// Tests tablet mode interaction without disabling the internal keyboard.
TEST_F(VirtualKeyboardControllerAutoTest, EnabledDuringTabletMode) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  // Toggle tablet mode on.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  // Toggle tablet mode off.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
}

// Tests that keyboard gets suppressed in tablet mode.
TEST_F(VirtualKeyboardControllerAutoTest, SuppressedInTabletMode) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard"));
  keyboard_devices.push_back(
      ui::KeyboardDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "Keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  // Toggle tablet mode on.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  keyboard_devices.pop_back();
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
  EXPECT_TRUE(notified());
  EXPECT_FALSE(IsVirtualKeyboardSuppressed());
  // Toggle tablet mode oFF.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(keyboard_ui_controller()->IsEnabled());
}

class VirtualKeyboardControllerAlwaysEnabledTest
    : public VirtualKeyboardControllerAutoTest {
 public:
  VirtualKeyboardControllerAlwaysEnabledTest()
      : VirtualKeyboardControllerAutoTest() {}

  VirtualKeyboardControllerAlwaysEnabledTest(
      const VirtualKeyboardControllerAlwaysEnabledTest&) = delete;
  VirtualKeyboardControllerAlwaysEnabledTest& operator=(
      const VirtualKeyboardControllerAlwaysEnabledTest&) = delete;

  ~VirtualKeyboardControllerAlwaysEnabledTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    VirtualKeyboardControllerAutoTest::SetUp();
  }
};

// Tests that the controller cannot suppress the keyboard if the virtual
// keyboard always enabled flag is active.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest, DoesNotSuppressKeyboard) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::KeyboardDevice> keyboard_devices;
  keyboard_devices.push_back(
      ui::KeyboardDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_TRUE(keyboard_ui_controller()->IsEnabled());
}

}  // namespace ash
