// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/overview_test_api.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}

namespace display {
class DisplayManager;
}

namespace ui {
class Accelerator;
}

namespace ash {
class DragDropController;
class MessageCenterController;
class NativeCursorManagerAsh;
class PowerPrefs;
class ScreenPositionController;
class Shell;
class WorkspaceController;

// Accesses private data from a Shell for testing.
class ASH_EXPORT ShellTestApi {
 public:
  ShellTestApi();

  ShellTestApi(const ShellTestApi&) = delete;
  ShellTestApi& operator=(const ShellTestApi&) = delete;

  ~ShellTestApi();

  // TabletModeController usually takes a screenshot before animating from
  // clamshell to tablet mode for performance reasons. This is an async
  // operation that we want to disable for most tests.
  static void SetTabletControllerUseScreenshotForTest(bool use_screenshot);

  // `SessionStateNotificationBlocker` adds a 6 second delays for showing all
  // non system component notifications after login. This behavior can cause
  // tests that expect to generate a notification to fail so should be disabled
  // for most tests. If a test deals specifically with this delay and needs to
  // set this to enabled, the test is responsible for setting it back to
  // disabled to prevent failing subsequent tests.
  static void SetUseLoginNotificationDelayForTest(bool use_delay);

  MessageCenterController* message_center_controller();
  WorkspaceController* workspace_controller();
  ScreenPositionController* screen_position_controller();
  NativeCursorManagerAsh* native_cursor_manager_ash();
  DragDropController* drag_drop_controller();
  PowerPrefs* power_prefs();
  display::DisplayManager* display_manager();

  // Resets |shell_->power_button_controller_| to hold a new object to simulate
  // Chrome starting.
  void ResetPowerButtonControllerForTest();

  // Simulates a modal dialog being open.
  void SimulateModalWindowOpenForTest(bool modal_window_open);

  // Returns true if a system modal window is open (e.g. the Wi-Fi network
  // password dialog).
  bool IsSystemModalWindowOpen();

  // Enables or disables the tablet mode. TabletMode switch can be
  // asynchronous, and this will wait until the transition is complete
  // by default.
  void SetTabletModeEnabledForTest(bool enable);

  // Enables the keyboard and associates it with the primary root window
  // controller. In tablet mode, enables the virtual keyboard.
  void EnableVirtualKeyboard();

  // Fullscreens the active window, as if the user had pressed the hardware
  // fullscreen button.
  void ToggleFullscreen();

  // Used to emulate display change when run in a desktop environment instead
  // of on a device.
  void AddRemoveDisplay();

  // Runs the callback when the overview state becomes |state|.
  void WaitForOverviewAnimationState(OverviewAnimationState state);

  void WaitForWindowFinishAnimating(aura::Window* window);

  // Returns true if the context menu associated with the primary root window is
  // shown.
  bool IsContextMenuShown() const;

  // Sends accelerator directly to AcceleratorController.
  bool IsActionForAcceleratorEnabled(const ui::Accelerator& accelerator) const;
  bool PressAccelerator(const ui::Accelerator& accelerator);

  // Returns true when Ash HUD is shown.
  bool IsHUDShown();

 private:
  raw_ptr<Shell> shell_;  // not owned
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_
