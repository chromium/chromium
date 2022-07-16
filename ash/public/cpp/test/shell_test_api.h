// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/overview_test_api.h"
#include "base/callback_forward.h"

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
enum class AppListViewState;
class DragDropController;
class MessageCenterController;
class NativeCursorManagerAsh;
class PaginationModel;
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

  // Whether a notification is shown at startup about new shortcuts. This
  // can interfere with tests that expect a certain window to be active,
  // that count notifications, or that test ChromeVox output.
  static void SetShouldShowShortcutNotificationForTest(bool show_notification);

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

  // Runs the callback when the WindowTreeHost of the primary display is no
  // longer holding pointer events. See
  // |aura::WindowTreeHost::holding_pointer_moves_| for details.
  void WaitForNoPointerHoldLock();

  // Runs the callback when the compositor of the primary display has presented
  // a frame on screen.
  void WaitForNextFrame(base::OnceClosure closure);

  // Runs the callback when the overview state becomes |state|.
  void WaitForOverviewAnimationState(OverviewAnimationState state);

  // Runs the callback when the launcher state becomes |state| after
  // state transition animation.
  void WaitForLauncherAnimationState(AppListViewState state);

  void WaitForWindowFinishAnimating(aura::Window* window);

  // Creates a closure that, when run, starts waiter for the window's current
  // animator to finish animating.
  // It can be used to wait for window animations when the window layer is
  // recreated while the animation is set up (as is the case for window hide
  // animations).
  // Example usage:
  //   base::OnceClosure waiter =
  //   CreateWaiterForFinishingWindowAnimation(window);
  //   aura::WindowState::Get(window)->Minimize();
  //   std::move(waiter).Run();
  base::OnceClosure CreateWaiterForFinishingWindowAnimation(
      aura::Window* window);

  // Returns the pagination model of the currently visible app-list view.
  // It returns nullptr when app-list is not shown.
  PaginationModel* GetAppListPaginationModel();

  // Returns true if the context menu associated with the primary root window is
  // shown.
  bool IsContextMenuShown() const;

  // Sends accelerator directly to AcceleratorController.
  bool IsActionForAcceleratorEnabled(const ui::Accelerator& accelerator) const;
  bool PressAccelerator(const ui::Accelerator& accelerator);

  // Returns true when Ash HUD is shown.
  bool IsHUDShown();

 private:
  Shell* shell_;  // not owned
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_
