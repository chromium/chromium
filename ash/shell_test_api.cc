// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "base/memory/raw_ptr.h"

#include <memory>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/hud_display/hud_display.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/autotest_private_api_utils.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/notification_center/session_state_notification_blocker.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/overview/overview_animation_state_waiter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/workspace_controller.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ash {
namespace {

class WindowAnimationWaiter : public ui::LayerAnimationObserver {
 public:
  explicit WindowAnimationWaiter(aura::Window* window)
      : animator_(window->layer()->GetAnimator()) {
    animator_->AddObserver(this);
  }
  ~WindowAnimationWaiter() override = default;

  WindowAnimationWaiter(const WindowAnimationWaiter& other) = delete;
  WindowAnimationWaiter& operator=(const WindowAnimationWaiter& rhs) = delete;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    if (!animator_->is_animating()) {
      animator_->RemoveObserver(this);
      run_loop_.Quit();
    }
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void Wait() { run_loop_.Run(); }

 private:
  raw_ptr<ui::LayerAnimator, DanglingUntriaged> animator_;
  base::RunLoop run_loop_;
};

}  // namespace

ShellTestApi::ShellTestApi() : shell_(Shell::Get()) {}
ShellTestApi::~ShellTestApi() = default;

// static
void ShellTestApi::SetTabletControllerUseScreenshotForTest(
    bool use_screenshot) {
  TabletModeController::SetUseScreenshotForTest(use_screenshot);
}

// static
void ShellTestApi::SetUseLoginNotificationDelayForTest(bool use_delay) {
  SessionStateNotificationBlocker::SetUseLoginNotificationDelayForTest(
      use_delay);
}

MessageCenterController* ShellTestApi::message_center_controller() {
  return shell_->message_center_controller_.get();
}

WorkspaceController* ShellTestApi::workspace_controller() {
  // TODO(afakhry): Split this into two, one for root, and one for context.
  return GetActiveWorkspaceController(shell_->GetPrimaryRootWindow());
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

display::DisplayManager* ShellTestApi::display_manager() {
  return shell_->display_manager();
}

void ShellTestApi::ResetPowerButtonControllerForTest() {
  shell_->backlights_forced_off_setter_->ResetForTest();
  shell_->power_button_controller_.reset();
  shell_->power_button_controller_ = std::make_unique<PowerButtonController>(
      shell_->backlights_forced_off_setter_.get());
}

void ShellTestApi::SimulateModalWindowOpenForTest(bool modal_window_open) {
  shell_->simulate_modal_window_open_for_test_ = modal_window_open;
}

bool ShellTestApi::IsSystemModalWindowOpen() {
  return Shell::IsSystemModalWindowOpen();
}

void ShellTestApi::SetTabletModeEnabledForTest(bool enable) {
  // Detach mouse devices, so we can enter tablet mode.
  // Calling RunUntilIdle() here is necessary before setting the mouse devices
  // to prevent the callback from evdev thread from overwriting whatever we set
  // here below. See `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
  base::RunLoop().RunUntilIdle();
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  ui::DeviceDataManagerTestApi().SetMouseDevices({});

  TabletMode::Waiter waiter(enable);
  shell_->tablet_mode_controller()->SetEnabledForTest(enable);
  waiter.Wait();
}

void ShellTestApi::EnableVirtualKeyboard() {
  shell_->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);
}

void ShellTestApi::ToggleFullscreen() {
  accelerators::ToggleFullscreen();
}

void ShellTestApi::AddRemoveDisplay() {
  shell_->display_manager()->AddRemoveDisplay();
}

void ShellTestApi::WaitForOverviewAnimationState(OverviewAnimationState state) {
  auto* overview_controller = shell_->overview_controller();
  if (state == OverviewAnimationState::kEnterAnimationComplete &&
      overview_controller->InOverviewSession() &&
      !overview_controller->IsInStartAnimation()) {
    // If there is no animation applied, call the callback immediately.
    return;
  }
  if (state == OverviewAnimationState::kExitAnimationComplete &&
      !overview_controller->InOverviewSession() &&
      !overview_controller->IsCompletingShutdownAnimations()) {
    // If there is no animation applied, call the callback immediately.
    return;
  }
  base::RunLoop run_loop;
  new OverviewAnimationStateWaiter(
      state, base::BindOnce([](base::RunLoop* run_loop,
                               bool finished) { run_loop->QuitWhenIdle(); },
                            base::Unretained(&run_loop)));
  run_loop.Run();
}

void ShellTestApi::WaitForWindowFinishAnimating(aura::Window* window) {
  WindowAnimationWaiter waiter(window);
  waiter.Wait();
}

bool ShellTestApi::IsContextMenuShown() const {
  return Shell::GetPrimaryRootWindowController()->IsContextMenuShownForTest();
}

bool ShellTestApi::IsActionForAcceleratorEnabled(
    const ui::Accelerator& accelerator) const {
  auto* controller = Shell::Get()->accelerator_controller();
  return AcceleratorControllerImpl::TestApi(controller)
      .IsActionForAcceleratorEnabled(accelerator);
}

bool ShellTestApi::PressAccelerator(const ui::Accelerator& accelerator) {
  return Shell::Get()->accelerator_controller()->AcceleratorPressed(
      accelerator);
}

bool ShellTestApi::IsHUDShown() {
  return hud_display::HUDDisplayView::IsShown();
}

}  // namespace ash
