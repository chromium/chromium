// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_test_api.h"

#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/ws/window_service_owner.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"

namespace ash {

ShellTestApi::ShellTestApi() : ShellTestApi(Shell::Get()) {}

ShellTestApi::ShellTestApi(Shell* shell) : shell_(shell) {}

// static
void ShellTestApi::BindRequest(mojom::ShellTestApiRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ShellTestApi>(), std::move(request));
}

MessageCenterController* ShellTestApi::message_center_controller() {
  return shell_->message_center_controller_.get();
}

SystemGestureEventFilter* ShellTestApi::system_gesture_event_filter() {
  return shell_->system_gesture_filter_.get();
}

WorkspaceController* ShellTestApi::workspace_controller() {
  return shell_->GetPrimaryRootWindowController()->workspace_controller();
}

ScreenPositionController* ShellTestApi::screen_position_controller() {
  return shell_->screen_position_controller_.get();
}

NativeCursorManagerAsh* ShellTestApi::native_cursor_manager_ash() {
  return shell_->native_cursor_manager_;
}

DragDropController* ShellTestApi::drag_drop_controller() {
  return shell_->drag_drop_controller_.get();
}

PowerPrefs* ShellTestApi::power_prefs() {
  return shell_->power_prefs_.get();
}

void ShellTestApi::OnLocalStatePrefServiceInitialized(
    std::unique_ptr<PrefService> pref_service) {
  shell_->OnLocalStatePrefServiceInitialized(std::move(pref_service));
}

void ShellTestApi::ResetPowerButtonControllerForTest() {
  shell_->backlights_forced_off_setter_->ResetForTest();
  shell_->power_button_controller_ = std::make_unique<PowerButtonController>(
      shell_->backlights_forced_off_setter_.get());
}

void ShellTestApi::SimulateModalWindowOpenForTest(bool modal_window_open) {
  shell_->simulate_modal_window_open_for_test_ = modal_window_open;
}

void ShellTestApi::IsSystemModalWindowOpen(IsSystemModalWindowOpenCallback cb) {
  std::move(cb).Run(Shell::IsSystemModalWindowOpen());
}

void ShellTestApi::EnableTabletModeWindowManager(bool enable) {
  shell_->tablet_mode_controller()->EnableTabletModeWindowManager(enable);
}

void ShellTestApi::EnableVirtualKeyboard(EnableVirtualKeyboardCallback cb) {
  shell_->EnableKeyboard();
  std::move(cb).Run();
}

void ShellTestApi::SnapWindowInSplitView(const std::string& client_name,
                                         ws::Id window_id,
                                         SnapWindowInSplitViewCallback cb) {
  auto* window_service = shell_->window_service_owner()->window_service();
  aura::Window* window = nullptr;
  for (ws::WindowTree* window_tree : window_service->window_trees()) {
    if (client_name == window_tree->client_name()) {
      window = window_tree->GetWindowByTransportId(window_id);
      break;
    }
  }
  DCHECK(window);
  shell_->split_view_controller()->SnapWindow(window,
                                              ash::SplitViewController::LEFT);
  shell_->split_view_controller()->FlushForTesting();
  std::move(cb).Run();
}

void ShellTestApi::ToggleFullscreen(ToggleFullscreenCallback cb) {
  ash::accelerators::ToggleFullscreen();
  std::move(cb).Run();
}

}  // namespace ash
