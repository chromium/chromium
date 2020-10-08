// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/ash_focus_manager_factory.h"
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/pre_target_accelerator_handler.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/key_accessibility_enabler.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/autoclick/autoclick_controller.h"
#include "ash/bloom/bloom_ui_controller_impl.h"
#include "ash/bloom/bloom_ui_delegate_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/child_accounts/parent_access_controller_impl.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/dbus/ash_dbus_services.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/detachable_base/detachable_base_notification_controller.h"
#include "ash/display/cros_display_config.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_alignment_controller.h"
#include "ash/display/display_color_manager.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_configuration_observer.h"
#include "ash/display/display_error_observer.h"
#include "ash/display/display_highlight_controller.h"
#include "ash/display/display_prefs.h"
#include "ash/display/display_shutdown_observer.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/persistent_window_controller.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/projecting_observer.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_ash.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/focus_cycler.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame/snap_controller_impl.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/hud_display/hud_display.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login_status.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media/media_controller_impl.h"
#include "ash/media/media_notification_controller_impl.h"
#include "ash/multi_device_setup/multi_device_notification_presenter.h"
#include "ash/policy/policy_recommendation_restorer.h"
#include "ash/power/peripheral_battery_tracker.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/quick_answers/quick_answers_controller_impl.h"
#include "ash/root_window_controller.h"
#include "ash/screenshot_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_init_params.h"
#include "ash/shell_observer.h"
#include "ash/shell_tab_handler.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/audio/display_speaker_controller.h"
#include "ash/system/bluetooth/bluetooth_notification_controller.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/bluetooth/tray_bluetooth_helper_experimental.h"
#include "ash/system/bluetooth/tray_bluetooth_helper_legacy.h"
#include "ash/system/brightness/brightness_controller_chromeos.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/message_center/message_center_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/nearby_share/nearby_share_controller_impl.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/peripheral_battery_notifier.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_event_observer.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/video_activity_notifier.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/screen_security/screen_switch_check_controller.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"
#include "ash/wm/immersive_context_ash.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/native_cursor_manager_ash.h"
#include "ash/wm/overlay_event_filter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_shadow_controller_delegate.h"
#include "ash/wm/workspace_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/components/bloom/public/cpp/bloom_controller.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/initialize_dbus_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/usb/usbguard_client.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/system/devicemode.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "dbus/bus.h"
#include "media/base/media_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/default_touch_transform_setter.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/touch_transform_setter.h"
#include "ui/display/screen.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"

namespace ash {

namespace {

using aura::Window;
using views::Widget;

// A Corewm VisibilityController subclass that calls the Ash animation routine
// so we can pick up our extended animations. See ash/wm/window_animations.h.
class AshVisibilityController : public ::wm::VisibilityController {
 public:
  AshVisibilityController() = default;
  ~AshVisibilityController() override = default;

 private:
  // Overridden from ::wm::VisibilityController:
  bool CallAnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) override {
    return AnimateOnChildWindowVisibilityChanged(window, visible);
  }

  DISALLOW_COPY_AND_ASSIGN(AshVisibilityController);
};

}  // namespace

// static
Shell* Shell::instance_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

// static
Shell* Shell::CreateInstance(ShellInitParams init_params) {
  CHECK(!instance_);
  instance_ = new Shell(std::move(init_params.delegate));
  instance_->Init(init_params.context_factory, init_params.local_state,
                  std::move(init_params.keyboard_ui_factory),
                  init_params.dbus_bus);
  return instance_;
}

// static
Shell* Shell::Get() {
  CHECK(instance_);
  return instance_;
}

// static
bool Shell::HasInstance() {
  return !!instance_;
}

// static
void Shell::DeleteInstance() {
  delete instance_;
}

// static
RootWindowController* Shell::GetPrimaryRootWindowController() {
  CHECK(HasInstance());
  return RootWindowController::ForWindow(GetPrimaryRootWindow());
}

// static
Shell::RootWindowControllerList Shell::GetAllRootWindowControllers() {
  CHECK(HasInstance());
  RootWindowControllerList root_window_controllers;
  for (aura::Window* root : GetAllRootWindows())
    root_window_controllers.push_back(RootWindowController::ForWindow(root));
  return root_window_controllers;
}

// static
RootWindowController* Shell::GetRootWindowControllerWithDisplayId(
    int64_t display_id) {
  CHECK(HasInstance());
  aura::Window* root = GetRootWindowForDisplayId(display_id);
  return root ? RootWindowController::ForWindow(root) : nullptr;
}

// static
aura::Window* Shell::GetRootWindowForDisplayId(int64_t display_id) {
  CHECK(HasInstance());
  return instance_->window_tree_host_manager_->GetRootWindowForDisplayId(
      display_id);
}

// static
aura::Window* Shell::GetPrimaryRootWindow() {
  CHECK(HasInstance());
  return instance_->window_tree_host_manager_->GetPrimaryRootWindow();
}

// static
void Shell::SetRootWindowForNewWindows(aura::Window* root) {
  display::Screen::GetScreen()->SetDisplayForNewWindows(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).id());
}

// static
aura::Window* Shell::GetRootWindowForNewWindows() {
  return GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetDisplayForNewWindows().id());
}

// static
aura::Window::Windows Shell::GetAllRootWindows() {
  CHECK(HasInstance());
  return instance_->window_tree_host_manager_->GetAllRootWindows();
}

// static
aura::Window* Shell::GetContainer(aura::Window* root_window, int container_id) {
  return root_window->GetChildById(container_id);
}

// static
const aura::Window* Shell::GetContainer(const aura::Window* root_window,
                                        int container_id) {
  return root_window->GetChildById(container_id);
}

// static
int Shell::GetOpenSystemModalWindowContainerId() {
  // The test boolean is not static to avoid leaking state between tests.
  if (Get()->simulate_modal_window_open_for_test_)
    return kShellWindowId_SystemModalContainer;

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (int modal_window_id : kSystemModalContainerIds) {
      aura::Window* system_modal = root->GetChildById(modal_window_id);
      if (!system_modal)
        continue;
      for (const aura::Window* child : system_modal->children()) {
        if (child->GetProperty(aura::client::kModalKey) ==
                ui::MODAL_TYPE_SYSTEM &&
            child->layer()->GetTargetVisibility()) {
          return modal_window_id;
        }
      }
    }
  }
  return -1;
}

// static
bool Shell::IsSystemModalWindowOpen() {
  return GetOpenSystemModalWindowContainerId() >= 0;
}

display::DisplayConfigurator* Shell::display_configurator() {
  return display_manager_->configurator();
}

void Shell::TrackInputMethodBounds(ArcInputMethodBoundsTracker* tracker) {
  system_tray_model()->virtual_keyboard()->SetInputMethodBoundsTrackerObserver(
      tracker);
}

void Shell::UntrackTrackInputMethodBounds(
    ArcInputMethodBoundsTracker* tracker) {
  system_tray_model()
      ->virtual_keyboard()
      ->RemoveInputMethodBoundsTrackerObserver(tracker);
}

std::unique_ptr<views::NonClientFrameView>
Shell::CreateDefaultNonClientFrameView(views::Widget* widget) {
  // Use translucent-style window frames for dialogs.
  return std::make_unique<NonClientFrameViewAsh>(widget);
}

void Shell::SetDisplayWorkAreaInsets(Window* contains,
                                     const gfx::Insets& insets) {
  window_tree_host_manager_->UpdateWorkAreaOfDisplayNearestWindow(contains,
                                                                  insets);
}

void Shell::OnCastingSessionStartedOrStopped(bool started) {
  for (auto& observer : shell_observers_)
    observer.OnCastingSessionStartedOrStopped(started);
}

void Shell::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnRootWindowAdded(root_window);
}

void Shell::OnDictationStarted() {
  for (auto& observer : shell_observers_)
    observer.OnDictationStarted();
}

void Shell::OnDictationEnded() {
  for (auto& observer : shell_observers_)
    observer.OnDictationEnded();
}

bool Shell::IsInTabletMode() const {
  return tablet_mode_controller()->InTabletMode();
}

bool Shell::ShouldSaveDisplaySettings() {
  return !(
      screen_orientation_controller_->ignore_display_configuration_updates() ||
      !display_configuration_observer_->save_preference());
}

DockedMagnifierControllerImpl* Shell::docked_magnifier_controller() {
  return docked_magnifier_controller_.get();
}

::wm::ActivationClient* Shell::activation_client() {
  return focus_controller_.get();
}

void Shell::UpdateShelfVisibility() {
  for (aura::Window* root : GetAllRootWindows())
    Shelf::ForWindow(root)->UpdateVisibilityState();
}

bool Shell::HasPrimaryStatusArea() {
  return !!GetPrimaryRootWindowController()->GetStatusAreaWidget();
}

void Shell::SetLargeCursorSizeInDip(int large_cursor_size_in_dip) {
  window_tree_host_manager_->cursor_window_controller()
      ->SetLargeCursorSizeInDip(large_cursor_size_in_dip);
}

void Shell::SetCursorColor(SkColor cursor_color) {
  window_tree_host_manager_->cursor_window_controller()->SetCursorColor(
      cursor_color);
}

void Shell::UpdateCursorCompositingEnabled() {
  SetCursorCompositingEnabled(
      window_tree_host_manager_->cursor_window_controller()
          ->ShouldEnableCursorCompositing());
}

void Shell::SetCursorCompositingEnabled(bool enabled) {
  CursorWindowController* cursor_window_controller =
      window_tree_host_manager_->cursor_window_controller();

  if (cursor_window_controller->is_cursor_compositing_enabled() == enabled)
    return;
  cursor_window_controller->SetCursorCompositingEnabled(enabled);
  native_cursor_manager_->SetNativeCursorEnabled(!enabled);
}

void Shell::DoInitialWorkspaceAnimation() {
  // Uses the active desk's workspace.
  auto* workspace = GetActiveWorkspaceController(GetPrimaryRootWindow());
  DCHECK(workspace);
  workspace->DoInitialAnimation();
}

void Shell::ShowContextMenu(const gfx::Point& location_in_screen,
                            ui::MenuSourceType source_type) {
  // Bail with no active user session, in the lock screen, or in app/kiosk mode.
  if (session_controller_->NumberOfLoggedInUsers() < 1 ||
      session_controller_->IsScreenLocked() ||
      session_controller_->IsRunningInAppMode()) {
    return;
  }

  aura::Window* root = window_util::GetRootWindowAt(location_in_screen);
  RootWindowController::ForWindow(root)->ShowContextMenu(location_in_screen,
                                                         source_type);
}

void Shell::AddShellObserver(ShellObserver* observer) {
  shell_observers_.AddObserver(observer);
}

void Shell::RemoveShellObserver(ShellObserver* observer) {
  shell_observers_.RemoveObserver(observer);
}

void Shell::UpdateAfterLoginStatusChange(LoginStatus status) {
  for (auto* root_window_controller : GetAllRootWindowControllers())
    root_window_controller->UpdateAfterLoginStatusChange(status);
}

void Shell::NotifyFullscreenStateChanged(bool is_fullscreen,
                                         aura::Window* container) {
  for (auto& observer : shell_observers_)
    observer.OnFullscreenStateChanged(is_fullscreen, container);
}

void Shell::NotifyPinnedStateChanged(aura::Window* pinned_window) {
  for (auto& observer : shell_observers_)
    observer.OnPinnedStateChanged(pinned_window);
}

void Shell::NotifyUserWorkAreaInsetsChanged(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnUserWorkAreaInsetsChanged(root_window);
}

void Shell::NotifyShelfAlignmentChanged(aura::Window* root_window,
                                        ShelfAlignment old_alignment) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAlignmentChanged(root_window, old_alignment);
}

void Shell::NotifyShelfAutoHideBehaviorChanged(aura::Window* root_window) {
  for (auto& observer : shell_observers_)
    observer.OnShelfAutoHideBehaviorChanged(root_window);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(std::unique_ptr<ShellDelegate> shell_delegate)
    : brightness_control_delegate_(
          std::make_unique<system::BrightnessControllerChromeos>()),
      focus_cycler_(std::make_unique<FocusCycler>()),
      ime_controller_(std::make_unique<ImeControllerImpl>()),
      immersive_context_(std::make_unique<ImmersiveContextAsh>()),
      in_session_auth_dialog_controller_(
          std::make_unique<InSessionAuthDialogControllerImpl>()),
      keyboard_brightness_control_delegate_(
          std::make_unique<KeyboardBrightnessController>()),
      locale_update_controller_(std::make_unique<LocaleUpdateControllerImpl>()),
      parent_access_controller_(std::make_unique<ParentAccessControllerImpl>()),
      session_controller_(std::make_unique<SessionControllerImpl>()),
      shell_delegate_(std::move(shell_delegate)),
      shutdown_controller_(std::make_unique<ShutdownControllerImpl>()),
      system_tray_notifier_(std::make_unique<SystemTrayNotifier>()),
      window_cycle_controller_(std::make_unique<WindowCycleController>()),
      native_cursor_manager_(nullptr) {
  // Ash doesn't properly remove pre-target-handlers.
  ui::EventHandler::DisableCheckTargets();

  // AccelerometerReader is important for screen orientation so we need
  // USER_VISIBLE priority.
  // Use CONTINUE_ON_SHUTDOWN to avoid blocking shutdown since the data reading
  // could get blocked on certain devices. See https://crbug.com/1023989.
  AccelerometerReader::GetInstance()->Initialize(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  login_screen_controller_ =
      std::make_unique<LoginScreenController>(system_tray_notifier_.get());
  display_manager_.reset(ScreenAsh::CreateDisplayManager());
  window_tree_host_manager_ = std::make_unique<WindowTreeHostManager>();
  user_metrics_recorder_ = std::make_unique<UserMetricsRecorder>();
  keyboard_controller_ =
      std::make_unique<KeyboardControllerImpl>(session_controller_.get());

  if (base::FeatureList::IsEnabled(features::kUseBluetoothSystemInAsh)) {
    mojo::PendingRemote<device::mojom::BluetoothSystemFactory>
        bluetooth_system_factory;
    shell_delegate_->BindBluetoothSystemFactory(
        bluetooth_system_factory.InitWithNewPipeAndPassReceiver());
    tray_bluetooth_helper_ = std::make_unique<TrayBluetoothHelperExperimental>(
        std::move(bluetooth_system_factory));
  } else {
    tray_bluetooth_helper_ = std::make_unique<TrayBluetoothHelperLegacy>();
  }

  PowerStatus::Initialize();

  session_controller_->AddObserver(this);
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");

  hud_display::HUDDisplayView::Destroy();

  for (auto& observer : shell_observers_)
    observer.OnShellDestroying();

  desks_controller_->Shutdown();

  user_metrics_recorder_->OnShellShuttingDown();

  cros_display_config_.reset();
  display_configuration_observer_.reset();
  display_prefs_.reset();
  display_alignment_controller_.reset();

  // Remove the focus from any window. This will prevent overhead and side
  // effects (e.g. crashes) from changing focus during shutdown.
  // See bug crbug.com/134502.
  aura::client::GetFocusClient(GetPrimaryRootWindow())->FocusWindow(nullptr);

  // Please keep in reverse order as in Init() because it's easy to miss one.
  if (window_modality_controller_)
    window_modality_controller_.reset();

  // We may shutdown while a capture session is active, which is an event
  // handler that depends on this shell and some of its members. Destroy early.
  capture_mode_controller_.reset();

  RemovePreTargetHandler(shell_tab_handler_.get());
  shell_tab_handler_.reset();

  RemovePreTargetHandler(magnifier_key_scroll_handler_.get());
  magnifier_key_scroll_handler_.reset();

  RemovePreTargetHandler(speech_feedback_handler_.get());
  speech_feedback_handler_.reset();

  RemovePreTargetHandler(overlay_filter_.get());
  overlay_filter_.reset();

  RemovePreTargetHandler(accelerator_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  if (back_gesture_event_handler_)
    RemovePreTargetHandler(back_gesture_event_handler_.get());
  RemovePreTargetHandler(toplevel_window_event_handler_.get());
  RemovePostTargetHandler(toplevel_window_event_handler_.get());
  RemovePreTargetHandler(system_gesture_filter_.get());
  RemovePreTargetHandler(mouse_cursor_filter_.get());
  RemovePreTargetHandler(modality_filter_.get());
  RemovePreTargetHandler(tooltip_controller_.get());

  event_rewriter_controller_.reset();

  screen_orientation_controller_.reset();
  screen_layout_observer_.reset();

  quick_answers_controller_.reset();

  // Destroy the virtual keyboard controller before the tablet mode controller
  // since the latters destructor triggers events that the former is listening
  // to but no longer cares about.
  keyboard_controller_->DestroyVirtualKeyboard();

  // Depends on |tablet_mode_controller_|.
  shelf_controller_->Shutdown();
  shelf_config_->Shutdown();

  // Destroy |home_screen_controller_| before |app_list_controller_| since
  // the former delegates to the latter.
  home_screen_controller_.reset();

  // Destroy |app_list_controller_| earlier than |tablet_mode_controller_| since
  // the former may use the latter before destruction.
  app_list_controller_.reset();

  // Accelerometer file reader stops listening to tablet mode controller.
  AccelerometerReader::GetInstance()->StopListenToTabletModeController();

  // Destroy |ambient_controller_| before |assistant_controller_|.
  ambient_controller_.reset();

  // Destroy |assistant_controller_| earlier than |tablet_mode_controller_| so
  // that the former will destroy the Assistant view hierarchy which has a
  // dependency on the latter.
  assistant_controller_.reset();

  // Because this function will call |TabletModeController::RemoveObserver|, do
  // it before destroying |tablet_mode_controller_|.
  accessibility_controller_->Shutdown();

  // Shutdown tablet mode controller early on since it has some observers which
  // need to be removed. It will be destroyed later after all windows are closed
  // since it might be accessed during this process.
  tablet_mode_controller_->Shutdown();

  // Destroy UserSettingsEventLogger before |system_tray_model_| and
  // |video_detector_| which it observes.
  ml::UserSettingsEventLogger::DeleteInstance();

  toast_manager_.reset();

  tray_bluetooth_helper_.reset();

  // Accesses root window containers.
  logout_confirmation_controller_.reset();

  // Drag-and-drop must be canceled prior to close all windows.
  for (aura::Window* root : GetAllRootWindows())
    aura::client::SetDragDropClient(root, nullptr);
  drag_drop_controller_.reset();

  // Controllers who have WindowObserver added must be deleted
  // before |window_tree_host_manager_| is deleted.
  persistent_window_controller_.reset();

  display_highlight_controller_.reset();

  // VideoActivityNotifier must be deleted before |video_detector_| is
  // deleted because it's observing video activity through
  // VideoDetector::Observer interface.
  video_activity_notifier_.reset();
  video_detector_.reset();
  high_contrast_controller_.reset();

  shadow_controller_.reset();
  resize_shadow_controller_.reset();

  // Has to happen before ~MruWindowTracker.
  window_cycle_controller_.reset();
  overview_controller_.reset();

  // Stop dispatching events (e.g. synthesized mouse exits from window close).
  // https://crbug.com/874156
  for (RootWindowController* rwc : GetAllRootWindowControllers())
    rwc->GetHost()->dispatcher()->Shutdown();

  // Close all widgets (including the shelf) and destroy all window containers.
  CloseAllRootWindowChildWindows();

  tablet_mode_controller_.reset();
  login_screen_controller_.reset();
  system_notification_controller_.reset();
  // Should be destroyed after Shelf and |system_notification_controller_|.
  system_tray_model_.reset();

  // MruWindowTracker must be destroyed after all windows have been deleted to
  // avoid a possible crash when Shell is destroyed from a non-normal shutdown
  // path. (crbug.com/485438).
  mru_window_tracker_.reset();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical. TODO(oshima): sort.
  autoclick_controller_.reset();
  magnification_controller_.reset();
  tooltip_controller_.reset();
  event_client_.reset();
  toplevel_window_event_handler_.reset();
  visibility_controller_.reset();
  power_prefs_.reset();

  tray_action_.reset();

  power_button_controller_.reset();
  lock_state_controller_.reset();
  backlights_forced_off_setter_.reset();

  screen_pinning_controller_.reset();

  multidevice_notification_presenter_.reset();
  resolution_notification_controller_.reset();
  screenshot_controller_.reset();
  mouse_cursor_filter_.reset();
  modality_filter_.reset();

  touch_transformer_controller_.reset();
  laser_pointer_controller_.reset();
  partial_magnification_controller_.reset();
  highlighter_controller_.reset();
  key_accessibility_enabler_.reset();

  display_speaker_controller_.reset();
  screen_switch_check_controller_.reset();

  ScreenAsh::CreateScreenForShutdown();
  display_configuration_controller_.reset();

  // These members access Shell in their destructors.
  wallpaper_controller_.reset();
  accessibility_controller_.reset();
  accessibility_delegate_.reset();
  accessibility_focus_ring_controller_.reset();
  policy_recommendation_restorer_.reset();
  ime_controller_.reset();
  back_gesture_event_handler_.reset();

  // Balances the Install() in Initialize().
  views::FocusManagerFactory::Install(nullptr);

  // ShelfWindowWatcher has window observers and a pointer to the shelf model.
  shelf_window_watcher_.reset();

  // Removes itself as an observer of |pref_service_|.
  shelf_controller_.reset();

  // NightLightControllerImpl depends on the PrefService as well as the window
  // tree host manager, and must be destructed before them. crbug.com/724231.
  night_light_controller_ = nullptr;
  // Similarly for DockedMagnifierControllerImpl.
  docked_magnifier_controller_ = nullptr;
  // Similarly for PrivacyScreenController.
  privacy_screen_controller_ = nullptr;

  // NearbyShareDelegateImpl must be destroyed before SessionController and
  // NearbyShareControllerImpl.
  nearby_share_delegate_.reset();
  nearby_share_controller_.reset();

  // Stop observing window activation changes before closing all windows.
  focus_controller_->RemoveObserver(this);

  // This also deletes all RootWindows. Note that we invoke Shutdown() on
  // WindowTreeHostManager before resetting |window_tree_host_manager_|, since
  // destruction of its owned RootWindowControllers relies on the value.
  window_tree_host_manager_->Shutdown();

  // Depends on |focus_controller_|, so must be destroyed before.
  window_tree_host_manager_.reset();

  // The desks controller is destroyed after the window tree host manager and
  // before the focus controller. At this point it is guaranteed that querying
  // the active desk is no longer needed.
  desks_controller_.reset();

  focus_rules_ = nullptr;
  focus_controller_.reset();
  screen_position_controller_.reset();

  display_color_manager_.reset();
  projecting_observer_.reset();

  if (display_change_observer_)
    display_manager_->configurator()->RemoveObserver(
        display_change_observer_.get());
  if (display_error_observer_)
    display_manager_->configurator()->RemoveObserver(
        display_error_observer_.get());
  display_change_observer_.reset();
  display_shutdown_observer_.reset();

  keyboard_controller_.reset();

  PowerStatus::Shutdown();
  // Depends on SessionController.
  power_event_observer_.reset();

  session_controller_->RemoveObserver(this);
  // BluetoothPowerController depends on the PrefService and must be destructed
  // before it.
  bluetooth_power_controller_ = nullptr;
  // TouchDevicesController depends on the PrefService and must be destructed
  // before it.
  touch_devices_controller_ = nullptr;
  // DetachableBaseNotificationController depends on DetachableBaseHandler, and
  // has to be destructed before it.
  detachable_base_notification_controller_.reset();
  // DetachableBaseHandler depends on the PrefService and must be destructed
  // before it.
  detachable_base_handler_.reset();

  // MediaNotificationControllerImpl depends on MessageCenter and must be
  // destructed before it.
  media_notification_controller_.reset();

  // Destroys the MessageCenter singleton, so must happen late.
  message_center_controller_.reset();

  // HoldingSpaceController observes SessionController and must be
  // destructed before it.
  holding_space_controller_.reset();

  ash_color_provider_.reset();

  shell_delegate_.reset();

  chromeos::UsbguardClient::Shutdown();

  // Must be shut down after detachable_base_handler_.
  chromeos::HammerdClient::Shutdown();

  for (auto& observer : shell_observers_)
    observer.OnShellDestroyed();

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(
    ui::ContextFactory* context_factory,
    PrefService* local_state,
    std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory,
    scoped_refptr<dbus::Bus> dbus_bus) {
  // Required by DetachableBaseHandler.
  chromeos::InitializeDBusClient<chromeos::HammerdClient>(dbus_bus.get());

  chromeos::InitializeDBusClient<chromeos::UsbguardClient>(dbus_bus.get());

  local_state_ = local_state;

  // This creates the MessageCenter object which is used by some other objects
  // initialized here, so it needs to come early.
  message_center_controller_ = std::make_unique<MessageCenterController>();

  // These controllers call Shell::Get() in their constructors, so they cannot
  // be in the member initialization list.
  touch_devices_controller_ = std::make_unique<TouchDevicesController>();
  bluetooth_power_controller_ =
      std::make_unique<BluetoothPowerController>(local_state_);
  detachable_base_handler_ =
      std::make_unique<DetachableBaseHandler>(local_state_);
  detachable_base_notification_controller_ =
      std::make_unique<DetachableBaseNotificationController>(
          detachable_base_handler_.get());
  display_speaker_controller_ = std::make_unique<DisplaySpeakerController>();
  policy_recommendation_restorer_ =
      std::make_unique<PolicyRecommendationRestorer>();
  screen_switch_check_controller_ =
      std::make_unique<ScreenSwitchCheckController>();
  multidevice_notification_presenter_ =
      std::make_unique<MultiDeviceNotificationPresenter>(
          message_center::MessageCenter::Get());
  media_controller_ = std::make_unique<MediaControllerImpl>();

  tablet_mode_controller_ = std::make_unique<TabletModeController>();

  accessibility_focus_ring_controller_ =
      std::make_unique<AccessibilityFocusRingControllerImpl>();
  accessibility_delegate_.reset(shell_delegate_->CreateAccessibilityDelegate());
  accessibility_controller_ = std::make_unique<AccessibilityControllerImpl>();
  toast_manager_ = std::make_unique<ToastManagerImpl>();

  if (features::IsCaptureModeEnabled()) {
    capture_mode_controller_ = std::make_unique<CaptureModeController>(
        shell_delegate_->CreateCaptureModeDelegate());
  }

  // Accelerometer file reader starts listening to tablet mode controller.
  AccelerometerReader::GetInstance()->StartListenToTabletModeController();

  // Install the custom factory early on so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  wallpaper_controller_ =
      std::make_unique<WallpaperControllerImpl>(local_state_);

  window_positioner_ = std::make_unique<WindowPositioner>();

  native_cursor_manager_ = new NativeCursorManagerAsh;
  cursor_manager_ =
      std::make_unique<CursorManager>(base::WrapUnique(native_cursor_manager_));

  InitializeDisplayManager();

  // RefreshFontParams depends on display prefs.
  display_manager_->RefreshFontParams();

  // This will initialize aura::Env which requires |display_manager_| to
  // be initialized first.
  aura::Env* env = aura::Env::GetInstance();
  if (context_factory)
    env->set_context_factory(context_factory);

  ash_color_provider_ = std::make_unique<AshColorProvider>();

  // Night Light depends on the display manager, the display color manager, and
  // aura::Env, so initialize it after all have been initialized.
  night_light_controller_ = std::make_unique<NightLightControllerImpl>();

  // Privacy Screen depends on the display manager, so initialize it after
  // display manager was properly initialized.
  privacy_screen_controller_ = std::make_unique<PrivacyScreenController>();

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_ =
      std::make_unique<::wm::WindowModalityController>(this, env);

  event_rewriter_controller_ = std::make_unique<EventRewriterControllerImpl>();

  env_filter_ = std::make_unique<::wm::CompoundEventFilter>();
  AddPreTargetHandler(env_filter_.get());

  // FocusController takes ownership of AshFocusRules.
  focus_rules_ = new AshFocusRules();
  focus_controller_ = std::make_unique<::wm::FocusController>(focus_rules_);
  focus_controller_->AddObserver(this);

  overview_controller_ = std::make_unique<OverviewController>();

  screen_position_controller_ = std::make_unique<ScreenPositionController>();

  window_tree_host_manager_->Start();
  AshWindowTreeHostInitParams ash_init_params;
  window_tree_host_manager_->CreatePrimaryHost(ash_init_params);

  // Create the desks controller right after the window tree host manager is
  // started, and before anything else is created, including the initialization
  // of the hosts and the root window controllers. Many things may need to query
  // the active desk, even at this early stage. For this the controller must be
  // present at all times. The desks controller also depends on the focus
  // controller.
  desks_controller_ = std::make_unique<DesksController>();

  Shell::SetRootWindowForNewWindows(GetPrimaryRootWindow());

  resolution_notification_controller_ =
      std::make_unique<ResolutionNotificationController>();

  cursor_manager_->SetDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay());

  accelerator_controller_ = std::make_unique<AcceleratorControllerImpl>();
  if (chromeos::features::IsClipboardHistoryEnabled()) {
    clipboard_history_controller_ =
        std::make_unique<ClipboardHistoryControllerImpl>();
    clipboard_history_controller_->Init();
  }
  shelf_config_ = std::make_unique<ShelfConfig>();
  shelf_controller_ = std::make_unique<ShelfController>();

  shell_tab_handler_ = std::make_unique<ShellTabHandler>(this);
  AddPreTargetHandler(shell_tab_handler_.get());
  magnifier_key_scroll_handler_ = MagnifierKeyScroller::CreateHandler();
  AddPreTargetHandler(magnifier_key_scroll_handler_.get());
  speech_feedback_handler_ = SpokenFeedbackToggler::CreateHandler();
  AddPreTargetHandler(speech_feedback_handler_.get());

  // The order in which event filters are added is significant.

  // ui::UserActivityDetector passes events to observers, so let them get
  // rewritten first.
  user_activity_detector_.reset(new ui::UserActivityDetector);

  overlay_filter_.reset(new OverlayEventFilter);
  AddPreTargetHandler(overlay_filter_.get());

  accelerator_filter_.reset(new ::wm::AcceleratorFilter(
      std::make_unique<PreTargetAcceleratorHandler>(),
      accelerator_controller_->accelerator_history()));
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_.reset(new EventTransformationHandler);
  AddPreTargetHandler(event_transformation_handler_.get());

  back_gesture_event_handler_ = std::make_unique<BackGestureEventHandler>();
  AddPreTargetHandler(back_gesture_event_handler_.get());

  toplevel_window_event_handler_ =
      std::make_unique<ToplevelWindowEventHandler>();

  system_gesture_filter_ = std::make_unique<SystemGestureEventFilter>();
  AddPreTargetHandler(system_gesture_filter_.get());

  sticky_keys_controller_.reset(new StickyKeysController);
  screen_pinning_controller_ = std::make_unique<ScreenPinningController>();

  power_prefs_ = std::make_unique<PowerPrefs>(
      chromeos::PowerPolicyController::Get(),
      chromeos::PowerManagerClient::Get(), local_state_);

  backlights_forced_off_setter_ = std::make_unique<BacklightsForcedOffSetter>();

  tray_action_ =
      std::make_unique<TrayAction>(backlights_forced_off_setter_.get());

  lock_state_controller_ =
      std::make_unique<LockStateController>(shutdown_controller_.get());
  power_button_controller_ = std::make_unique<PowerButtonController>(
      backlights_forced_off_setter_.get());
  // Pass the initial display state to PowerButtonController.
  power_button_controller_->OnDisplayModeChanged(
      display_configurator()->cached_displays());

  drag_drop_controller_ = std::make_unique<DragDropController>();

  // |screenshot_controller_| needs to be created (and prepended as a
  // pre-target handler) at this point, because |mouse_cursor_filter_| needs to
  // process mouse events prior to screenshot session.
  // See http://crbug.com/459214
  screenshot_controller_ = std::make_unique<ScreenshotController>(
      shell_delegate_->CreateScreenshotDelegate());
  mouse_cursor_filter_ = std::make_unique<MouseCursorEventFilter>();
  AddPreTargetHandler(mouse_cursor_filter_.get(),
                      ui::EventTarget::Priority::kAccessibility);

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_.reset(new AshVisibilityController);

  laser_pointer_controller_.reset(new LaserPointerController());
  partial_magnification_controller_.reset(new PartialMagnificationController());
  highlighter_controller_.reset(new HighlighterController());

  magnification_controller_ = std::make_unique<MagnificationController>();
  mru_window_tracker_ = std::make_unique<MruWindowTracker>();
  assistant_controller_ = std::make_unique<AssistantControllerImpl>();
  if (chromeos::features::IsQuickAnswersEnabled() &&
      chromeos::features::IsQuickAnswersRichUiEnabled()) {
    quick_answers_controller_ = std::make_unique<QuickAnswersControllerImpl>();
  }

  // |assistant_controller_| is put before |ambient_controller_| as it will be
  // used by the latter.
  if (chromeos::features::IsAmbientModeEnabled()) {
    mojo::PendingRemote<device::mojom::Fingerprint> fingerprint;
    shell_delegate_->BindFingerprint(
        fingerprint.InitWithNewPipeAndPassReceiver());
    ambient_controller_ =
        std::make_unique<AmbientController>(std::move(fingerprint));
  }

  if (chromeos::assistant::features::IsBloomEnabled())
    bloom_ui_controller_ = std::make_unique<BloomUiControllerImpl>();

  home_screen_controller_ = std::make_unique<HomeScreenController>();

  // |tablet_mode_controller_| |mru_window_tracker_|,
  // |assistant_controller_| and |home_screen_controller_| are put before
  // |app_list_controller_| as they are used in its constructor.
  app_list_controller_ = std::make_unique<AppListControllerImpl>();
  home_screen_controller_->SetDelegate(app_list_controller_.get());

  autoclick_controller_ = std::make_unique<AutoclickController>();

  high_contrast_controller_.reset(new HighContrastController);

  docked_magnifier_controller_ =
      std::make_unique<DockedMagnifierControllerImpl>();

  video_detector_ = std::make_unique<VideoDetector>();

  tooltip_controller_.reset(new views::corewm::TooltipController(
      std::unique_ptr<views::corewm::Tooltip>(new views::corewm::TooltipAura)));
  AddPreTargetHandler(tooltip_controller_.get());

  modality_filter_.reset(new SystemModalContainerEventFilter(this));
  AddPreTargetHandler(modality_filter_.get());

  event_client_.reset(new EventClientImpl);

  resize_shadow_controller_.reset(new ResizeShadowController());
  shadow_controller_ = std::make_unique<::wm::ShadowController>(
      focus_controller_.get(), std::make_unique<WmShadowControllerDelegate>(),
      env);

  logout_confirmation_controller_ =
      std::make_unique<LogoutConfirmationController>();

  // May trigger initialization of the Bluetooth adapter.
  tray_bluetooth_helper_->Initialize();

  // Create AshTouchTransformController before
  // WindowTreeHostManager::InitDisplays()
  // since AshTouchTransformController listens on
  // WindowTreeHostManager::Observer::OnDisplaysInitialized().
  touch_transformer_controller_ = std::make_unique<AshTouchTransformController>(
      display_manager_.get(),
      std::make_unique<display::DefaultTouchTransformSetter>());

  // |system_tray_model_| should be available before
  // |system_notification_controller_| is initialized and Shelf is created by
  // WindowTreeHostManager::InitHosts.
  system_tray_model_ = std::make_unique<SystemTrayModel>();

  // The |shelf_config_| needs |app_list_controller_| and |system_tray_model_|
  // to initialize itself.
  shelf_config_->Init();

  nearby_share_controller_ = std::make_unique<NearbyShareControllerImpl>();
  nearby_share_delegate_ = shell_delegate_->CreateNearbyShareDelegate(
      nearby_share_controller_.get());

  system_notification_controller_ =
      std::make_unique<SystemNotificationController>();

  window_tree_host_manager_->InitHosts();

  // Create virtual keyboard after WindowTreeHostManager::InitHosts() since
  // it may enable the virtual keyboard immediately, which requires a
  // WindowTreeHostManager to host the keyboard window.
  keyboard_controller_->CreateVirtualKeyboard(std::move(keyboard_ui_factory));

  cursor_manager_->HideCursor();  // Hide the mouse cursor on startup.
  cursor_manager_->SetCursor(ui::mojom::CursorType::kPointer);

  peripheral_battery_notifier_ = std::make_unique<PeripheralBatteryNotifier>();
  if (base::FeatureList::IsEnabled(
          chromeos::features::kShowBluetoothDeviceBattery)) {
    peripheral_battery_tracker_ = std::make_unique<PeripheralBatteryTracker>();
  }
  power_event_observer_.reset(new PowerEventObserver());

  mojo::PendingRemote<device::mojom::Fingerprint> fingerprint;
  shell_delegate_->BindFingerprint(
      fingerprint.InitWithNewPipeAndPassReceiver());
  user_activity_notifier_ =
      std::make_unique<ui::UserActivityPowerManagerNotifier>(
          user_activity_detector_.get(), std::move(fingerprint));
  video_activity_notifier_.reset(
      new VideoActivityNotifier(video_detector_.get()));
  bluetooth_notification_controller_ =
      std::make_unique<BluetoothNotificationController>(
          message_center::MessageCenter::Get());
  screen_orientation_controller_ =
      std::make_unique<ScreenOrientationController>();

  cros_display_config_ = std::make_unique<CrosDisplayConfig>();

  screen_layout_observer_.reset(new ScreenLayoutObserver());
  sms_observer_.reset(new SmsObserver());
  snap_controller_ = std::make_unique<SnapControllerImpl>();
  key_accessibility_enabler_ = std::make_unique<KeyAccessibilityEnabler>();
  frame_throttling_controller_ =
      std::make_unique<FrameThrottlingController>(context_factory);

  // Create UserSettingsEventLogger after |system_tray_model_| and
  // |video_detector_| which it observes.
  ml::UserSettingsEventLogger::CreateInstance();

  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  display_manager_->CreateMirrorWindowAsyncIfAny();

  if (base::FeatureList::IsEnabled(features::kMediaSessionNotification) &&
      !base::FeatureList::IsEnabled(media::kGlobalMediaControlsForChromeOS)) {
    media_notification_controller_ =
        std::make_unique<MediaNotificationControllerImpl>();
  }

  // TODO(1091497): Consider combining DisplayHighlightController and
  // DisplayAlignmentController.
  if (features::IsDisplayIdentificationEnabled()) {
    display_highlight_controller_ =
        std::make_unique<DisplayHighlightController>();
  }

  if (features::IsDisplayAlignmentAssistanceEnabled()) {
    display_alignment_controller_ =
        std::make_unique<DisplayAlignmentController>();
  }

  if (features::IsTemporaryHoldingSpaceEnabled())
    holding_space_controller_ = std::make_unique<HoldingSpaceController>();

  for (auto& observer : shell_observers_)
    observer.OnShellInitialized();

  user_metrics_recorder_->OnShellInitialized();

  // Initialize the D-Bus bus and services for ash.
  dbus_bus_ = dbus_bus;
  ash_dbus_services_ = std::make_unique<AshDBusServices>(dbus_bus.get());

  // By this point ash shell should have initialized its D-Bus signal
  // listeners, so inform the session manager that Ash is initialized.
  session_controller_->EmitAshInitialized();
}

void Shell::InitializeDisplayManager() {
  display_manager_->InitConfigurator(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate());
  display_configuration_controller_ =
      std::make_unique<DisplayConfigurationController>(
          display_manager_.get(), window_tree_host_manager_.get());
  display_configuration_observer_ =
      std::make_unique<DisplayConfigurationObserver>();

  persistent_window_controller_ =
      std::make_unique<PersistentWindowController>();

  projecting_observer_ =
      std::make_unique<ProjectingObserver>(display_manager_->configurator());

  display_prefs_ = std::make_unique<DisplayPrefs>(local_state_);

  bool display_initialized = display_manager_->InitFromCommandLine();

  if (!display_initialized) {
    if (chromeos::IsRunningAsSystemCompositor()) {
      display_change_observer_ =
          std::make_unique<display::DisplayChangeObserver>(
              display_manager_.get());

      display_error_observer_ = std::make_unique<DisplayErrorObserver>();
      display_shutdown_observer_ = std::make_unique<DisplayShutdownObserver>(
          display_manager_->configurator());

      display_manager_->ForceInitialConfigureWithObservers(
          display_change_observer_.get(), display_error_observer_.get());
      display_initialized = true;
    }
  }

  display_color_manager_ = std::make_unique<DisplayColorManager>(
      display_manager_->configurator(), display::Screen::GetScreen());

  if (!display_initialized)
    display_manager_->InitDefaultDisplay();
}

void Shell::InitRootWindow(aura::Window* root_window) {
  DCHECK(focus_controller_);
  DCHECK(visibility_controller_.get());

  aura::client::SetFocusClient(root_window, focus_controller_.get());
  ::wm::SetActivationClient(root_window, focus_controller_.get());
  root_window->AddPreTargetHandler(focus_controller_.get());
  aura::client::SetVisibilityClient(root_window, visibility_controller_.get());
  aura::client::SetDragDropClient(root_window, drag_drop_controller_.get());
  aura::client::SetScreenPositionClient(root_window,
                                        screen_position_controller_.get());
  aura::client::SetCursorClient(root_window, cursor_manager_.get());
  ::wm::SetTooltipClient(root_window, tooltip_controller_.get());
  aura::client::SetEventClient(root_window, event_client_.get());

  ::wm::SetWindowMoveClient(root_window, toplevel_window_event_handler_.get());
  root_window->AddPreTargetHandler(toplevel_window_event_handler_.get());
  root_window->AddPostTargetHandler(toplevel_window_event_handler_.get());
}

void Shell::CloseAllRootWindowChildWindows() {
  for (aura::Window* root : GetAllRootWindows()) {
    RootWindowController* controller = RootWindowController::ForWindow(root);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root->children().empty()) {
        aura::Window* child = root->children()[0];
        delete child;
      }
    }
  }
}

bool Shell::CanWindowReceiveEvents(aura::Window* window) {
  RootWindowControllerList controllers = GetAllRootWindowControllers();
  for (RootWindowController* controller : controllers) {
    if (controller->CanWindowReceiveEvents(window))
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, ui::EventTarget overrides:

bool Shell::CanAcceptEvent(const ui::Event& event) {
  return true;
}

ui::EventTarget* Shell::GetParentTarget() {
  return aura::Env::GetInstance();
}

std::unique_ptr<ui::EventTargetIterator> Shell::GetChildIterator() const {
  return std::unique_ptr<ui::EventTargetIterator>();
}

ui::EventTargeter* Shell::GetEventTargeter() {
  NOTREACHED();
  return nullptr;
}

void Shell::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active)
    return;

  Shell::SetRootWindowForNewWindows(gained_active->GetRootWindow());
}

void Shell::OnFirstSessionStarted() {
  // Enable magnifier scroll keys as there may be no mouse cursor in kiosk mode.
  MagnifierKeyScroller::SetEnabled(session_controller_->IsRunningInAppMode());

  // Enable long press action to toggle spoken feedback with hotrod remote
  // which can't handle shortcuts.
  SpokenFeedbackToggler::SetEnabled(session_controller_->IsRunningInAppMode());

  // Reset user prefs related to contextual tooltips.
  if (switches::ContextualNudgesResetShownCount())
    contextual_tooltip::ClearPrefs();
}

void Shell::OnSessionStateChanged(session_manager::SessionState state) {
  const bool is_session_active = state == session_manager::SessionState::ACTIVE;
  // Initialize the |shelf_window_watcher_| when a session becomes active.
  // Shelf itself is initialized in RootWindowController.
  if (is_session_active && !shelf_window_watcher_) {
    shelf_window_watcher_ =
        std::make_unique<ShelfWindowWatcher>(shelf_controller()->model());
  }

  // Disable drag-and-drop during OOBE and GAIA login screens by only enabling
  // the controller when the session is active. https://crbug.com/464118
  drag_drop_controller_->set_enabled(is_session_active);
}

void Shell::OnLoginStatusChanged(LoginStatus login_status) {
  UpdateAfterLoginStatusChange(login_status);
}

void Shell::OnLockStateChanged(bool locked) {
#ifndef NDEBUG
  // Make sure that there is no system modal in Lock layer when unlocked.
  if (!locked) {
    aura::Window::Windows containers = GetContainersForAllRootWindows(
        kShellWindowId_LockSystemModalContainer, GetPrimaryRootWindow());
    for (aura::Window* container : containers)
      DCHECK(container->children().empty());
  }
#endif
}

}  // namespace ash
