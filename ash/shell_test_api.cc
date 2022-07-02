// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/hud_display/hud_display.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/autotest_private_api_utils.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/message_center/session_state_notification_blocker.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/overview/overview_animation_state_waiter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ash {
namespace {

// Wait for a WindowTreeHost to no longer be holding pointer events.
class PointerMoveLoopWaiter : public ui::CompositorObserver {
 public:
  explicit PointerMoveLoopWaiter(aura::WindowTreeHost* window_tree_host)
      : window_tree_host_(window_tree_host) {
    window_tree_host_->compositor()->AddObserver(this);
  }

  PointerMoveLoopWaiter(const PointerMoveLoopWaiter&) = delete;
  PointerMoveLoopWaiter& operator=(const PointerMoveLoopWaiter&) = delete;

  ~PointerMoveLoopWaiter() override {
    window_tree_host_->compositor()->RemoveObserver(this);
  }

  void Wait() {
    // Use a while loop as it's possible for releasing the lock to trigger
    // processing events, which again grabs the lock.
    while (window_tree_host_->holding_pointer_moves()) {
      run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // ui::CompositorObserver:
  void OnCompositingEnded(ui::Compositor* compositor) override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  aura::WindowTreeHost* window_tree_host_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

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
  ui::LayerAnimator* animator_;
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

void ShellTestApi::WaitForNoPointerHoldLock() {
  aura::WindowTreeHost* primary_host =
      Shell::GetPrimaryRootWindowController()->GetHost();
  if (primary_host->holding_pointer_moves())
    PointerMoveLoopWaiter(primary_host).Wait();
}

void ShellTestApi::WaitForNextFrame(base::OnceClosure closure) {
  Shell::GetPrimaryRootWindowController()
      ->GetHost()
      ->compositor()
      ->RequestPresentationTimeForNextFrame(base::BindOnce(
          [](base::OnceClosure closure,
             const gfx::PresentationFeedback& feedback) {
            std::move(closure).Run();
          },
          std::move(closure)));
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

void ShellTestApi::WaitForLauncherAnimationState(
    AppListViewState target_state) {
  base::RunLoop run_loop;
  WaitForLauncherState(target_state, run_loop.QuitWhenIdleClosure());
  run_loop.Run();
}

void ShellTestApi::WaitForWindowFinishAnimating(aura::Window* window) {
  WindowAnimationWaiter waiter(window);
  waiter.Wait();
}

base::OnceClosure ShellTestApi::CreateWaiterForFinishingWindowAnimation(
    aura::Window* window) {
  auto waiter = std::make_unique<WindowAnimationWaiter>(window);
  return base::BindOnce(&WindowAnimationWaiter::Wait, std::move(waiter));
}

PaginationModel* ShellTestApi::GetAppListPaginationModel() {
  AppListView* view =
      Shell::Get()->app_list_controller()->fullscreen_presenter()->GetView();
  if (!view)
    return nullptr;
  return view->GetAppsPaginationModel();
}

bool ShellTestApi::IsContextMenuShown() const {
  return Shell::GetPrimaryRootWindowController()->IsContextMenuShown();
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
