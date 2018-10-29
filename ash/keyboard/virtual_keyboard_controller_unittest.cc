// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/virtual_keyboard_controller.h"

#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller.h"
#include "ash/ime/test_ime_controller_client.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"

using keyboard::mojom::KeyboardEnableFlag;

namespace ash {

namespace {

VirtualKeyboardController* GetVirtualKeyboardController() {
  return Shell::Get()->ash_keyboard_controller()->virtual_keyboard_controller();
}

}  // namespace

class VirtualKeyboardControllerTest : public AshTestBase {
 public:
  VirtualKeyboardControllerTest() = default;
  ~VirtualKeyboardControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    ws::InputDeviceClientTestApi().SetKeyboardDevices({});
    ws::InputDeviceClientTestApi().SetTouchscreenDevices({});
    keyboard_controller_ = keyboard::KeyboardController::Get();
  }

  void TearDown() override {
    // Ensure inputs devices are reset for the next test.
    ws::InputDeviceClientTestApi().SetKeyboardDevices({});
    ws::InputDeviceClientTestApi().SetTouchscreenDevices({});

    AshTestBase::TearDown();
  }

  display::Display GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay();
  }

  display::Display GetSecondaryDisplay() {
    return Shell::Get()->display_manager()->GetSecondaryDisplay();
  }

  aura::Window* GetPrimaryRootWindow() { return Shell::GetPrimaryRootWindow(); }

  aura::Window* GetSecondaryRootWindow() {
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    return root_windows[0] == GetPrimaryRootWindow() ? root_windows[1]
                                                     : root_windows[0];
  }

  void CreateFocusedTestWindowInRootWindow(aura::Window* root_window) {
    // Owned by |root_window|.
    aura::Window* focusable_window =
        CreateTestWindowInShellWithBounds(root_window->GetBoundsInScreen());
    focusable_window->Focus();
  }

  keyboard::KeyboardController* keyboard_controller_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerTest);
};

// Mock event blocker that enables the internal keyboard when it's destructor
// is called.
class MockEventBlocker : public ScopedDisableInternalMouseAndKeyboard {
 public:
  MockEventBlocker() = default;
  ~MockEventBlocker() override {
    std::vector<ui::InputDevice> keyboard_devices;
    keyboard_devices.push_back(ui::InputDevice(
        1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
    ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventBlocker);
};

// Tests that reenabling keyboard devices while shutting down does not
// cause the Virtual Keyboard Controller to crash. See crbug.com/446204.
TEST_F(VirtualKeyboardControllerTest, RestoreKeyboardDevices) {
  // Toggle tablet mode on.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  std::unique_ptr<ScopedDisableInternalMouseAndKeyboard> blocker(
      new MockEventBlocker);
  TabletModeControllerTestApi().set_event_blocker(std::move(blocker));
}

TEST_F(VirtualKeyboardControllerTest,
       ForceToShowKeyboardWithKeysetWhenAccessibilityKeyboardIsEnabled) {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  accessibility_controller->SetVirtualKeyboardEnabled(true);
  ASSERT_TRUE(accessibility_controller->IsVirtualKeyboardEnabled());

  // Set up a mock ImeControllerClient to test keyset changes.
  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(client.CreateInterfacePtr());

  // Should show the keyboard without messing with accessibility prefs.
  GetVirtualKeyboardController()->ForceShowKeyboardWithKeyset(
      chromeos::input_method::mojom::ImeKeyset::kEmoji);
  Shell::Get()->ime_controller()->FlushMojoForTesting();
  EXPECT_TRUE(accessibility_controller->IsVirtualKeyboardEnabled());

  // Keyset should be emoji.
  Shell::Get()->ime_controller()->FlushMojoForTesting();
  EXPECT_EQ(chromeos::input_method::mojom::ImeKeyset::kEmoji,
            client.last_keyset_);

  // Simulate the keyboard hiding.
  if (keyboard_controller_->HasObserver(GetVirtualKeyboardController())) {
    GetVirtualKeyboardController()->OnKeyboardHidden(
        false /* is_temporary_hide */);
  }
  base::RunLoop().RunUntilIdle();

  // The keyboard should still be enabled.
  EXPECT_TRUE(accessibility_controller->IsVirtualKeyboardEnabled());

  // Reset the accessibility prefs.
  accessibility_controller->SetVirtualKeyboardEnabled(false);

  // Keyset should be reset to none.
  Shell::Get()->ime_controller()->FlushMojoForTesting();
  EXPECT_EQ(chromeos::input_method::mojom::ImeKeyset::kNone,
            client.last_keyset_);
}

TEST_F(VirtualKeyboardControllerTest,
       ForceToShowKeyboardWithKeysetWhenKeyboardIsDisabled) {
  // Set up a mock ImeControllerClient to test keyset changes.
  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(client.CreateInterfacePtr());

  // Should show the keyboard by enabling it temporarily.
  EXPECT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  EXPECT_FALSE(
      keyboard_controller_->IsEnableFlagSet(KeyboardEnableFlag::kShelfEnabled));

  GetVirtualKeyboardController()->ForceShowKeyboardWithKeyset(
      chromeos::input_method::mojom::ImeKeyset::kEmoji);
  Shell::Get()->ime_controller()->FlushMojoForTesting();

  EXPECT_TRUE(
      keyboard_controller_->IsEnableFlagSet(KeyboardEnableFlag::kShelfEnabled));
  EXPECT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());

  // Keyset should be emoji.
  EXPECT_EQ(chromeos::input_method::mojom::ImeKeyset::kEmoji,
            client.last_keyset_);

  // Simulate the keyboard hiding.
  if (keyboard_controller_->HasObserver(GetVirtualKeyboardController())) {
    GetVirtualKeyboardController()->OnKeyboardHidden(
        false /* is_temporary_hide */);
  }
  base::RunLoop().RunUntilIdle();

  // The keyboard should still be disabled again.
  EXPECT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  EXPECT_FALSE(
      keyboard_controller_->IsEnableFlagSet(KeyboardEnableFlag::kShelfEnabled));

  // Keyset should be reset to none.
  Shell::Get()->ime_controller()->FlushMojoForTesting();
  EXPECT_EQ(chromeos::input_method::mojom::ImeKeyset::kNone,
            client.last_keyset_);
}

TEST_F(VirtualKeyboardControllerTest,
       ForceToShowKeyboardWithKeysetTemporaryHide) {
  // Set up a mock ImeControllerClient to test keyset changes.
  TestImeControllerClient client;
  Shell::Get()->ime_controller()->SetClient(client.CreateInterfacePtr());

  // Should show the keyboard by enabling it temporarily.
  GetVirtualKeyboardController()->ForceShowKeyboardWithKeyset(
      chromeos::input_method::mojom::ImeKeyset::kEmoji);
  Shell::Get()->ime_controller()->FlushMojoForTesting();

  EXPECT_TRUE(
      keyboard_controller_->IsEnableFlagSet(KeyboardEnableFlag::kShelfEnabled));
  EXPECT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());

  // Keyset should be emoji.
  EXPECT_EQ(chromeos::input_method::mojom::ImeKeyset::kEmoji,
            client.last_keyset_);

  // Simulate the keyboard hiding temporarily.
  if (keyboard_controller_->HasObserver(GetVirtualKeyboardController())) {
    GetVirtualKeyboardController()->OnKeyboardHidden(
        true /* is_temporary_hide */);
  }
  base::RunLoop().RunUntilIdle();

  // The keyboard should still be enabled.
  EXPECT_TRUE(
      keyboard_controller_->IsEnableFlagSet(KeyboardEnableFlag::kShelfEnabled));
  EXPECT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());

  // Keyset should still be emoji.
  EXPECT_EQ(chromeos::input_method::mojom::ImeKeyset::kEmoji,
            client.last_keyset_);
}

class VirtualKeyboardControllerAutoTest : public VirtualKeyboardControllerTest,
                                          public VirtualKeyboardObserver {
 public:
  VirtualKeyboardControllerAutoTest() : notified_(false), suppressed_(false) {}
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

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerAutoTest);
};

// Tests that the onscreen keyboard is disabled if an internal keyboard is
// present and tablet mode is disabled.
TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfInternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ws::InputDeviceClientTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  // Remove the internal keyboard. Virtual keyboard should now show.
  ws::InputDeviceClientTestApi().SetKeyboardDevices({});
  EXPECT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  // Replug in the internal keyboard. Virtual keyboard should hide.
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  EXPECT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
}

TEST_F(VirtualKeyboardControllerAutoTest, DisabledIfNoTouchScreen) {
  std::vector<ui::TouchscreenDevice> devices;
  // Add a touchscreen. Keyboard should deploy.
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB,
                            "Touchscreen", gfx::Size(800, 600), 0));
  ws::InputDeviceClientTestApi().SetTouchscreenDevices(devices);
  EXPECT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  // Remove touchscreen. Keyboard should hide.
  ws::InputDeviceClientTestApi().SetTouchscreenDevices({});
  EXPECT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
}

TEST_F(VirtualKeyboardControllerAutoTest, SuppressedIfExternalKeyboardPresent) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(ui::TouchscreenDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Touchscreen",
      gfx::Size(1024, 768), 0, false /* has_stylus */));
  ws::InputDeviceClientTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(
      ui::InputDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  ASSERT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  ws::InputDeviceClientTestApi().SetKeyboardDevices({});
  ASSERT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_FALSE(IsVirtualKeyboardSuppressed());
}

// Tests handling multiple keyboards. Catches crbug.com/430252
TEST_F(VirtualKeyboardControllerAutoTest, HandleMultipleKeyboardsPresent) {
  std::vector<ui::InputDevice> keyboards;
  keyboards.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard"));
  keyboards.push_back(
      ui::InputDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  keyboards.push_back(
      ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboards);
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
}

// Tests tablet mode interaction without disabling the internal keyboard.
TEST_F(VirtualKeyboardControllerAutoTest, EnabledDuringTabletMode) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ws::InputDeviceClientTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard"));
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  // Toggle tablet mode on.
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  // Toggle tablet mode off.
  TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
}

// Tests that keyboard gets suppressed in tablet mode.
TEST_F(VirtualKeyboardControllerAutoTest, SuppressedInTabletMode) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ws::InputDeviceClientTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(ui::InputDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "Keyboard"));
  keyboard_devices.push_back(
      ui::InputDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "Keyboard"));
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  // Toggle tablet mode on.
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be visible.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  ASSERT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Toggle show keyboard. Keyboard should be hidden.
  ResetObserver();
  GetVirtualKeyboardController()->ToggleIgnoreExternalKeyboard();
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_TRUE(IsVirtualKeyboardSuppressed());
  // Remove external keyboard. Should be notified that the keyboard is not
  // suppressed.
  ResetObserver();
  keyboard_devices.pop_back();
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  ASSERT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
  ASSERT_TRUE(notified());
  ASSERT_FALSE(IsVirtualKeyboardSuppressed());
  // Toggle tablet mode oFF.
  TabletModeControllerTestApi().LeaveTabletMode();
  ASSERT_FALSE(keyboard_controller_->IsKeyboardEnableRequested());
}

class VirtualKeyboardControllerAlwaysEnabledTest
    : public VirtualKeyboardControllerAutoTest {
 public:
  VirtualKeyboardControllerAlwaysEnabledTest()
      : VirtualKeyboardControllerAutoTest() {}
  ~VirtualKeyboardControllerAlwaysEnabledTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    VirtualKeyboardControllerAutoTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardControllerAlwaysEnabledTest);
};

// Tests that the controller cannot suppress the keyboard if the virtual
// keyboard always enabled flag is active.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest, DoesNotSuppressKeyboard) {
  std::vector<ui::TouchscreenDevice> screens;
  screens.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "Touchscreen", gfx::Size(1024, 768), 0));
  ws::InputDeviceClientTestApi().SetTouchscreenDevices(screens);
  std::vector<ui::InputDevice> keyboard_devices;
  keyboard_devices.push_back(
      ui::InputDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard"));
  ws::InputDeviceClientTestApi().SetKeyboardDevices(keyboard_devices);
  ASSERT_TRUE(keyboard_controller_->IsKeyboardEnableRequested());
}

// Test for http://crbug.com/297858. |MoveKeyboardToTouchableDisplay| should
// move keyboard to primary display if no display has touch capability and
// no window is focused.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       MovesKeyboardToPrimaryDisplayWhenNoDisplayHasTouch) {
  UpdateDisplay("500x500,500x500");

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  GetVirtualKeyboardController()->MoveKeyboardToTouchableDisplay();

  EXPECT_EQ(GetPrimaryRootWindow(), keyboard_controller_->GetRootWindow());
}

// Test for http://crbug.com/297858. |MoveKeyboardToTouchableDisplay| should
// move keyboard to focused display if no display has touch capability.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       MovesKeyboardToFocusedDisplayWhenNoDisplayHasTouch) {
  UpdateDisplay("500x500,500x500");

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  CreateFocusedTestWindowInRootWindow(GetSecondaryRootWindow());

  GetVirtualKeyboardController()->MoveKeyboardToTouchableDisplay();

  EXPECT_EQ(GetSecondaryRootWindow(), keyboard_controller_->GetRootWindow());
}

// Test for http://crbug.com/303429. |MoveKeyboardToTouchableDisplay| should
// move keyboard to first touchable display when there is one.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       MovesKeyboardToFirstTouchableDisplay) {
  UpdateDisplay("500x500,500x500");

  // Make secondary display touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetSecondaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  GetVirtualKeyboardController()->MoveKeyboardToTouchableDisplay();

  EXPECT_EQ(GetSecondaryRootWindow(), keyboard_controller_->GetRootWindow());
}

// Test for http://crbug.com/303429. |MoveKeyboardToTouchableDisplay| should
// move keyboard to first touchable display when the focused display is not
// touchable.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       MovesKeyboardToFirstTouchableDisplayIfFocusedDisplayIsNotTouchable) {
  UpdateDisplay("500x500,500x500");

  // Make secondary display touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetSecondaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  // Focus on primary display.
  CreateFocusedTestWindowInRootWindow(GetPrimaryRootWindow());

  GetVirtualKeyboardController()->MoveKeyboardToTouchableDisplay();
  EXPECT_EQ(GetSecondaryRootWindow(), keyboard_controller_->GetRootWindow());
}

// Test for http://crbug.com/303429. |MoveKeyboardToTouchableDisplay| should
// move keyborad to first touchable display when there is one.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       MovesKeyboardToFocusedDisplayIfTouchable) {
  UpdateDisplay("500x500,500x500");

  // Make both displays touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetPrimaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetSecondaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  // Focus on secondary display.
  CreateFocusedTestWindowInRootWindow(GetSecondaryRootWindow());
  GetVirtualKeyboardController()->MoveKeyboardToTouchableDisplay();
  EXPECT_EQ(GetSecondaryRootWindow(), keyboard_controller_->GetRootWindow());

  // Focus on primary display.
  CreateFocusedTestWindowInRootWindow(GetPrimaryRootWindow());
  GetVirtualKeyboardController()->MoveKeyboardToTouchableDisplay();
  EXPECT_EQ(GetPrimaryRootWindow(), keyboard_controller_->GetRootWindow());
}

// Test for http://crbug.com/303429. |MoveKeyboardToDisplay| should move
// keyboard to specified display even when it's not touchable.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       MovesKeyboardToSpecifiedDisplay) {
  UpdateDisplay("500x500,500x500");

  // Make primary display touchable.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetTouchSupport(GetPrimaryDisplay().id(),
                       display::Display::TouchSupport::AVAILABLE);

  EXPECT_EQ(display::Display::TouchSupport::AVAILABLE,
            GetPrimaryDisplay().touch_support());
  EXPECT_NE(display::Display::TouchSupport::AVAILABLE,
            GetSecondaryDisplay().touch_support());

  // Move to primary display.
  GetVirtualKeyboardController()->MoveKeyboardToDisplay(GetPrimaryDisplay());
  EXPECT_EQ(GetPrimaryRootWindow(), keyboard_controller_->GetRootWindow());

  // Move to secondary display.
  GetVirtualKeyboardController()->MoveKeyboardToDisplay(GetSecondaryDisplay());
  EXPECT_EQ(GetSecondaryRootWindow(), keyboard_controller_->GetRootWindow());
}

// Test for https://crbug.com/897007.
TEST_F(VirtualKeyboardControllerAlwaysEnabledTest,
       ShowKeyboardInSecondaryDisplay) {
  UpdateDisplay("500x500,500x500");

  // Load in the primary display.
  keyboard_controller_->LoadKeyboardWindowInBackground();
  keyboard_controller_->GetKeyboardWindow()->SetBounds(gfx::Rect(0, 0, 10, 10));
  keyboard_controller_->NotifyKeyboardWindowLoaded();

  // Show in secondary display.
  keyboard_controller_->ShowKeyboardInDisplay(GetSecondaryDisplay());
  EXPECT_EQ(GetSecondaryRootWindow(), keyboard_controller_->GetRootWindow());
  ASSERT_TRUE(keyboard::WaitUntilShown());
  EXPECT_TRUE(!keyboard_controller_->GetKeyboardWindow()->bounds().IsEmpty());
}

}  // namespace ash
