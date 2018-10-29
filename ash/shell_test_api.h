// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_TEST_API_H_
#define ASH_SHELL_TEST_API_H_

#include <memory>

#include "ash/public/interfaces/shell_test_api.mojom.h"
#include "base/macros.h"
#include "services/ws/common/types.h"

class PrefService;

namespace ash {
class DragDropController;
class MessageCenterController;
class NativeCursorManagerAsh;
class PowerPrefs;
class ScreenPositionController;
class Shell;
class SystemGestureEventFilter;
class WorkspaceController;

// Accesses private data from a Shell for testing.
class ShellTestApi : public mojom::ShellTestApi {
 public:
  ShellTestApi();
  explicit ShellTestApi(Shell* shell);

  // Creates and binds an instance from a remote request (e.g. from chrome).
  static void BindRequest(mojom::ShellTestApiRequest request);

  MessageCenterController* message_center_controller();
  SystemGestureEventFilter* system_gesture_event_filter();
  WorkspaceController* workspace_controller();
  ScreenPositionController* screen_position_controller();
  NativeCursorManagerAsh* native_cursor_manager_ash();
  DragDropController* drag_drop_controller();
  PowerPrefs* power_prefs();

  // Calls the private method.
  void OnLocalStatePrefServiceInitialized(
      std::unique_ptr<PrefService> pref_service);

  // Resets |shell_->power_button_controller_| to hold a new object to simulate
  // Chrome starting.
  void ResetPowerButtonControllerForTest();

  // Simulates a modal dialog being open.
  void SimulateModalWindowOpenForTest(bool modal_window_open);

  // mojom::ShellTestApi:
  void IsSystemModalWindowOpen(IsSystemModalWindowOpenCallback cb) override;
  void EnableTabletModeWindowManager(bool enable) override;
  void EnableVirtualKeyboard(EnableVirtualKeyboardCallback cb) override;
  void SnapWindowInSplitView(const std::string& client_name,
                             ws::Id window_id,
                             SnapWindowInSplitViewCallback cb) override;
  void ToggleFullscreen(ToggleFullscreenCallback cb) override;

 private:
  Shell* shell_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(ShellTestApi);
};

}  // namespace ash

#endif  // ASH_SHELL_TEST_API_H_
