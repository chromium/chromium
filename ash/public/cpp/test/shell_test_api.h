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
#include "base/macros.h"

namespace aura {
class Window;
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
class SystemGestureEventFilter;
class WorkspaceController;

// Accesses private data from a Shell for testing.
class ASH_EXPORT ShellTestApi {
 public:
  ShellTestApi();
  ~ShellTestApi();

  // TabletModeController usually takes a screenshot before animating from
  // clamshell to tablet mode for performance reasons. This is an async
  // operation that we want to disable for most tests.
  static void SetTabletControllerUseScreenshotForTest(bool use_screenshot);

  MessageCenterController* message_center_controller();
  SystemGestureEventFilter* system_gesture_event_filter();
  WorkspaceController* workspace_controller();
  ScreenPositionController* screen_position_controller();
  NativeCursorManagerAsh* native_cursor_manager_ash();
  DragDropController* drag_drop_controller();
  PowerPrefs* power_prefs();

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
  // by default. Set |wait_for_completion| to false if you do not want
  // to wait.
  void SetTabletModeEnabledForTest(bool enable,
                                   bool wait_for_completion = true);

  // Enables the keyboard and associates it with the primary root window
  // controller. In tablet mode, enables the virtual keyboard.
  void EnableVirtualKeyboard();

  // Fullscreens the active window, as if the user had pressed the hardware
  // fullscreen button.
  void ToggleFullscreen();

  // Returns true if it is in overview selecting mode.
  bool IsOverviewSelecting();

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

  // Returns the pagination model of the currently visible app-list view.
  // It returns nullptr when app-list is not shown.
  PaginationModel* GetAppListPaginationModel();

  // Returns the list of windows used in overview item. Returns empty
  // if not in the overview mode.
  std::vector<aura::Window*> GetItemWindowListInOverviewGrids();

 private:
  Shell* shell_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(ShellTestApi);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_SHELL_TEST_API_H_
