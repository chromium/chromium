// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/shell.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_lookup.h"
#include "ash/accelerators/accelerator_prefs.h"
#include "ash/accelerators/accelerator_tracker.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/accelerators/ash_focus_manager_factory.h"
#include "ash/accelerators/magnifier_key_scroller.h"
#include "ash/accelerators/modifier_key_combo_recorder.h"
#include "ash/accelerators/pre_target_accelerator_handler.h"
#include "ash/accelerators/rapid_key_sequence_recorder.h"
#include "ash/accelerators/shortcut_input_handler.h"
#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/accessibility/chromevox/key_accessibility_enabler.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/magnifier/partial_magnifier_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/annotator/annotator_controller.h"
#include "ash/api/tasks/tasks_controller.h"
#include "ash/api/tasks/tasks_delegate.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_feature_usage_metrics.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/auth/active_session_auth_controller_impl.h"
#include "ash/birch/birch_model.h"
#include "ash/booting/booting_animation_controller.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/child_accounts/parent_access_controller_impl.h"
#include "ash/clipboard/clipboard_history_controller_delegate.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/control_v_histogram_recorder.h"
#include "ash/color_enhancement/color_enhancement_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/curtain/security_curtain_controller_impl.h"
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
#include "ash/display/display_performance_mode_controller.h"
#include "ash/display/display_prefs.h"
#include "ash/display/display_shutdown_observer.h"
#include "ash/display/event_transformation_handler.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/projecting_observer.h"
#include "ash/display/refresh_rate_controller.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_ash.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/focus_cycler.h"
#include "ash/frame/multitask_menu_nudge_delegate_ash.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame/snap_controller_impl.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/post_login_glanceables_metrics_recorder.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/hud_display/hud_display.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"
#include "ash/in_session_auth/webauthn_dialog_controller_impl.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ash/lobster/lobster_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/local_authentication_request_controller_impl.h"
#include "ash/login_status.h"
#include "ash/media/media_controller_impl.h"
#include "ash/metrics/feature_discovery_duration_reporter_impl.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/metrics/unlock_throughput_recorder.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_capture/multi_capture_service_client.h"
#include "ash/multi_device_setup/multi_device_notification_presenter.h"
#include "ash/picker/picker_controller.h"
#include "ash/policy/policy_recommendation_restorer.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/public/cpp/nearby_share_delegate.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/views_text_services_context_menu_ash.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/root_window_controller.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell_delegate.h"
#include "ash/shell_init_params.h"
#include "ash/shell_observer.h"
#include "ash/shell_tab_handler.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/style/ash_color_mixer.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/style_util.h"
#include "ash/system/audio/audio_effects_controller.h"
#include "ash/system/audio/display_speaker_controller.h"
#include "ash/system/bluetooth/bluetooth_device_status_ui_handler.h"
#include "ash/system/bluetooth/bluetooth_notification_controller.h"
#include "ash/system/bluetooth/bluetooth_state_cache.h"
#include "ash/system/brightness/brightness_controller_chromeos.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/caps_lock_notification_controller.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "ash/system/firmware_update/firmware_update_notification_controller.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/hotspot/hotspot_icon_animation.h"
#include "ash/system/hotspot/hotspot_info_cache.h"
#include "ash/system/human_presence/human_presence_orientation_controller.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "ash/system/input_device_settings/input_device_key_alias_manager.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_dispatcher.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/keyboard_modifier_metrics_recorder.h"
#include "ash/system/input_device_settings/touchscreen_metrics_recorder.h"
#include "ash/system/keyboard_brightness/keyboard_backlight_color_controller.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_controller.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/system/nearby_share/nearby_share_controller_impl.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/notification_center/message_center_ash_impl.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/pcie_peripheral/pcie_peripheral_notification_controller.h"
#include "ash/system/power/adaptive_charging_controller.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/peripheral_battery_notifier.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_event_observer.h"
#include "ash/system/power/power_prefs.h"
#include "ash/system/power/power_status.h"
#include "ash/system/power/video_activity_notifier.h"
#include "ash/system/privacy/screen_switch_check_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/system_notification_controller.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/toast/system_nudge_pause_manager_impl.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/usb_peripheral/usb_peripheral_notification_controller.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/tray_action/tray_action.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/user_education/user_education_delegate.h"
#include "ash/utility/occlusion_tracker_pauser.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "ash/wm/event_client_impl.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"
#include "ash/wm/immersive_context_ash.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/multi_display/multi_display_metrics_controller.h"
#include "ash/wm/multi_display/persistent_window_controller.h"
#include "ash/wm/native_cursor_manager_ash.h"
#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/raster_scale/raster_scale_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/system_gesture_event_filter.h"
#include "ash/wm/system_modal_container_event_filter.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/tile_group/window_tiling_controller.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/video_detector.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_shadow_controller_delegate.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm_mode/wm_mode_controller.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/audio/system_sounds_delegate.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/dbus/typecd/typecd_client.h"
#include "chromeos/ash/components/dbus/usb/usbguard_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/init/initialize_dbus_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "dbus/bus.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "services/video_capture/public/mojom/multi_capture_service.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/default_touch_transform_setter.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_port_observer.h"
#include "ui/display/manager/touch_transform_setter.h"
#include "ui/display/screen.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/controls/views_text_services_context_menu_chromeos.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/accelerator_filter.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"

namespace ash {

namespace {

using aura::Window;
using views::Widget;

constexpr int kTooltipMaxWidth = 296;

// A Corewm VisibilityController subclass that calls the Ash animation routine
// so we can pick up our extended animations. See ash/wm/window_animations.h.
class AshVisibilityController : public ::wm::VisibilityController {
 public:
  AshVisibilityController() = default;

  AshVisibilityController(const AshVisibilityController&) = delete;
  AshVisibilityController& operator=(const AshVisibilityController&) = delete;

  ~AshVisibilityController() override = default;

 private:
  // Overridden from ::wm::VisibilityController:
  bool CallAnimateOnChildWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) override {
    return AnimateOnChildWindowVisibilityChanged(window, visible);
  }
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
                  std::move(init_params.quick_pair_mediator_factory),
                  init_params.dbus_bus,
                  std::move(init_params.native_display_delegate));
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
  for (aura::Window* root : GetAllRootWindows()) {
    root_window_controllers.push_back(RootWindowController::ForWindow(root));
  }
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
  if (Get()->simulate_modal_window_open_for_test_) {
    return kShellWindowId_SystemModalContainer;
  }

  // Traverse all system modal containers, and find its direct child window
  // with "SystemModal" setting, and visible.
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    for (int modal_window_id : kSystemModalContainerIds) {
      aura::Window* system_modal = root->GetChildById(modal_window_id);
      if (!system_modal) {
        continue;
      }
      for (const aura::Window* child : system_modal->children()) {
        if (child->GetProperty(aura::client::kModalKey) ==
                ui::mojom::ModalType::kSystem &&
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

void Shell::OnCastingSessionStartedOrStopped(bool started) {
  for (auto& observer : shell_observers_) {
    observer.OnCastingSessionStartedOrStopped(started);
  }
}

void Shell::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& observer : shell_observers_) {
    observer.OnRootWindowAdded(root_window);
  }
}

void Shell::OnRootWindowWillShutdown(aura::Window* root_window) {
  DCHECK(toplevel_window_event_handler_);
  root_window->RemovePreTargetHandler(toplevel_window_event_handler_.get());
  root_window->RemovePostTargetHandler(toplevel_window_event_handler_.get());

  for (auto& observer : shell_observers_) {
    observer.OnRootWindowWillShutdown(root_window);
  }
}

void Shell::OnDictationStarted() {
  for (auto& observer : shell_observers_) {
    observer.OnDictationStarted();
  }
}

void Shell::OnDictationEnded() {
  for (auto& observer : shell_observers_) {
    observer.OnDictationEnded();
  }
}

bool Shell::IsInTabletMode() const {
  return display::Screen::GetScreen()->InTabletMode();
}

bool Shell::ShouldSaveDisplaySettings() {
  return !(
      screen_orientation_controller_->ignore_display_configuration_updates() ||
      // Save display settings if we don't need to show the display change
      // dialog.
      resolution_notification_controller_->ShouldShowDisplayChangeDialog());
}

::wm::ActivationClient* Shell::activation_client() {
  return focus_controller_.get();
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

  if (cursor_window_controller->is_cursor_compositing_enabled() == enabled) {
    return;
  }
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

void Shell::ShutdownEventDispatch() {
  for (aura::Window* root : GetAllRootWindows()) {
    aura::client::SetDragDropClient(root, nullptr);
  }

  // Stop dispatching events (e.g. synthesized mouse exits from window close).
  // https://crbug.com/874156
  for (RootWindowController* rwc : GetAllRootWindowControllers()) {
    rwc->GetHost()->dispatcher()->Shutdown();
  }
}

void Shell::UpdateAfterLoginStatusChange(LoginStatus status) {
  for (auto* root_window_controller : GetAllRootWindowControllers()) {
    root_window_controller->UpdateAfterLoginStatusChange(status);
  }
}

void Shell::NotifyFullscreenStateChanged(bool is_fullscreen,
                                         aura::Window* container) {
  for (auto& observer : shell_observers_) {
    observer.OnFullscreenStateChanged(is_fullscreen, container);
  }
}

void Shell::NotifyPinnedStateChanged(aura::Window* pinned_window) {
  for (auto& observer : shell_observers_) {
    observer.OnPinnedStateChanged(pinned_window);
  }
}

void Shell::NotifyUserWorkAreaInsetsChanged(aura::Window* root_window) {
  for (auto& observer : shell_observers_) {
    observer.OnUserWorkAreaInsetsChanged(root_window);
  }
}

void Shell::NotifyShelfAlignmentChanged(aura::Window* root_window,
                                        ShelfAlignment old_alignment) {
  for (auto& observer : shell_observers_) {
    observer.OnShelfAlignmentChanged(root_window, old_alignment);
  }
}

void Shell::NotifyDisplayForNewWindowsChanged() {
  for (auto& observer : shell_observers_) {
    observer.OnDisplayForNewWindowsChanged();
  }
}

void Shell::AddAccessibilityEventHandler(
    ui::EventHandler* handler,
    AccessibilityEventHandlerManager::HandlerType type) {
  if (!accessibility_event_handler_manager_) {
    accessibility_event_handler_manager_ =
        std::make_unique<AccessibilityEventHandlerManager>();
  }

  accessibility_event_handler_manager_->AddAccessibilityEventHandler(handler,
                                                                     type);
}
void Shell::RemoveAccessibilityEventHandler(ui::EventHandler* handler) {
  accessibility_event_handler_manager_->RemoveAccessibilityEventHandler(
      handler);
}

DeskProfilesDelegate* Shell::GetDeskProfilesDelegate() {
  return shell_delegate_->GetDeskProfilesDelegate();
}

WebAuthNDialogController* Shell::webauthn_dialog_controller() {
  return webauthn_dialog_controller_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

Shell::Shell(std::unique_ptr<ShellDelegate> shell_delegate)
    : focus_cycler_(std::make_unique<FocusCycler>()),
      ime_controller_(std::make_unique<ImeControllerImpl>()),
      immersive_context_(std::make_unique<ImmersiveContextAsh>()),
      webauthn_dialog_controller_(
          std::make_unique<WebAuthNDialogControllerImpl>()),
      in_session_auth_dialog_controller_(
          std::make_unique<InSessionAuthDialogControllerImpl>()),
      locale_update_controller_(std::make_unique<LocaleUpdateControllerImpl>()),
      parent_access_controller_(std::make_unique<ParentAccessControllerImpl>()),
      local_authentication_request_controller_(
          std::make_unique<LocalAuthenticationRequestControllerImpl>()),
      active_session_auth_controller_(
          std::make_unique<ActiveSessionAuthControllerImpl>()),
      session_controller_(std::make_unique<SessionControllerImpl>()),
      feature_discover_reporter_(
          std::make_unique<FeatureDiscoveryDurationReporterImpl>(
              session_controller_.get())),
      shell_delegate_(std::move(shell_delegate)),
      shutdown_controller_(std::make_unique<ShutdownControllerImpl>()),
      system_tray_notifier_(std::make_unique<SystemTrayNotifier>()),
      native_cursor_manager_(nullptr) {
  AccelerometerReader::GetInstance()->Initialize();

  login_screen_controller_ =
      std::make_unique<LoginScreenController>(system_tray_notifier_.get());
  display_manager_ = ScreenAsh::CreateDisplayManager();
  window_tree_host_manager_ = std::make_unique<WindowTreeHostManager>();
  user_metrics_recorder_ = std::make_unique<UserMetricsRecorder>();
  keyboard_controller_ =
      std::make_unique<KeyboardControllerImpl>(session_controller_.get());

  PowerStatus::Initialize();

  session_controller_->AddObserver(this);
  keyboard_controller_->AddObserver(ime_controller_.get());
}

Shell::~Shell() {
  TRACE_EVENT0("shutdown", "ash::Shell::Destructor");
#if DCHECK_IS_ON()
  // All WindowEventDispatchers should be shutdown before the Shell is
  // destroyed.
  for (RootWindowController* rwc : GetAllRootWindowControllers()) {
    DCHECK(rwc->GetHost()->dispatcher()->in_shutdown());
  }
#endif
  booting_animation_controller_.reset();
  unlock_throughput_recorder_.reset();
  login_unlock_throughput_recorder_.reset();

  hud_display::HUDDisplayView::Destroy();

  // Observes `SessionController` and must be destroyed before it.
  privacy_hub_controller_.reset();

  for (auto& observer : shell_observers_) {
    observer.OnShellDestroying();
  }

  ash_dbus_services_.reset();

  coral_delegate_.reset();

  saved_desk_controller_.reset();
  saved_desk_delegate_.reset();
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
  if (window_modality_controller_) {
    window_modality_controller_.reset();
  }

  RemovePreTargetHandler(shell_tab_handler_.get());
  shell_tab_handler_.reset();

  RemovePreTargetHandler(magnifier_key_scroll_handler_.get());
  magnifier_key_scroll_handler_.reset();

  RemovePreTargetHandler(speech_feedback_handler_.get());
  speech_feedback_handler_.reset();

  RemovePreTargetHandler(control_v_histogram_recorder_.get());
  RemovePreTargetHandler(accelerator_tracker_.get());
  RemovePreTargetHandler(accelerator_filter_.get());
  RemovePreTargetHandler(event_transformation_handler_.get());
  if (back_gesture_event_handler_) {
    RemovePreTargetHandler(back_gesture_event_handler_.get());
  }
  RemovePreTargetHandler(system_gesture_filter_.get());
  RemoveAccessibilityEventHandler(mouse_cursor_filter_.get());
  if (features::IsPeripheralCustomizationEnabled() ||
      ::features::IsShortcutCustomizationEnabled()) {
    RemovePreTargetHandler(shortcut_input_handler_.get());
  }
  RemovePreTargetHandler(modality_filter_.get());
  if (::features::IsAccessibilityMouseKeysEnabled()) {
    RemovePreTargetHandler(mouse_keys_controller_.get());
  }
  RemovePreTargetHandler(tooltip_controller_.get());

  // Resets the implementation of clipboard history utility functions.
  chromeos::clipboard_history::SetQueryItemDescriptorsImpl(
      base::NullCallback());
  chromeos::clipboard_history::SetPasteClipboardItemByIdImpl(
      base::NullCallback());

  // Resets the text context menu implementation factory.
  views::ViewsTextServicesContextMenuChromeos::SetImplFactory(
      base::NullCallback());

  wm_mode_controller_.reset();

  // `shortcut_input_handler_` must be cleaned up before
  // `event_rewriter_controller_`.
  modifier_key_combo_recorder_.reset();
  rapid_key_sequence_recorder_.reset();
  shortcut_input_handler_.reset();
  // `AccessibilityEventRewriter` references objects owned by
  // EventRewriterController directly, so it must be reset first to avoid
  // accessing invalid memory (see b/315127220).
  AccessibilityController::Get()->SetAccessibilityEventRewriter(nullptr);
  AccessibilityController::Get()->SetDisableTrackpadEventRewriter(nullptr);
  AccessibilityController::Get()->SetFilterKeysEventRewriter(nullptr);
  // AccessibilityController observes
  // input_device_settings_controller_; it also outlives
  // input_device_settings_controller_, so we need to explicitly stop observing
  // to ensure proper teardown.
  AccessibilityController::Get()->StopObservingInputDeviceSettings();
  event_rewriter_controller_.reset();
  keyboard_modifier_metrics_recorder_.reset();
  touchscreen_metrics_recorder_.reset();
  input_device_settings_dispatcher_.reset();
  input_device_tracker_.reset();
  input_device_settings_controller_.reset();
  input_device_key_alias_manager_.reset();

  screen_orientation_controller_.reset();
  screen_layout_observer_.reset();

  keyboard_controller_->RemoveObserver(ime_controller_.get());
  // Destroy the virtual keyboard controller before the tablet mode controller
  // since the latters destructor triggers events that the former is listening
  // to but no longer cares about.
  keyboard_controller_->DestroyVirtualKeyboard();
  picker_controller_.reset();

  // Depends on |tablet_mode_controller_|.
  window_restore_controller_.reset();
  shelf_controller_->Shutdown();
  shelf_config_->Shutdown();

  birch_privacy_nudge_controller_.reset();
  birch_model_.reset();

  // Depends on `app_list_controller_` and `tablet_mode_controller_`.
  app_list_feature_usage_metrics_.reset();

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

  // Must be destructed before human_presence_orientation_controller_.
  power_prefs_.reset();

  // Must be destructed before the tablet mode and message center controllers,
  // both of which these rely on.
  snooping_protection_controller_.reset();
  human_presence_orientation_controller_.reset();

  snap_group_controller_.reset();

  // Shutdown tablet mode controller early on since it has some observers which
  // need to be removed. It will be destroyed later after all windows are closed
  // since it might be accessed during this process.
  tablet_mode_controller_->Shutdown();

  // Shutdown the clipboard history controller to clean up the child windows and
  // widgets that may be animating out.
  clipboard_history_controller_->Shutdown();

  toast_manager_.reset();
  anchored_nudge_manager_.reset();

  // Accesses root window containers.
  logout_confirmation_controller_.reset();

  adaptive_charging_controller_.reset();

  // Drag-and-drop must be canceled prior to close all windows.
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
  color_enhancement_controller_.reset();

  shadow_controller_.reset();
  resize_shadow_controller_.reset();
  raster_scale_controller_.reset();

  // Has to happen before ~MruWindowTracker.
  window_cycle_controller_.reset();

  // As a client of `projector_controller_` and `capture_mode_controller_`,
  // `annotator_controller_` needs to be destroyed first.
  annotator_controller_.reset();

  // As clients of `capture_mode_controller_`, `projector_controller_` and
  // `game_dashboard_controller_` need to be destroyed before
  // `capture_mode_controller_`.
  projector_controller_.reset();
  game_dashboard_controller_.reset();

  // This must be called before `capture_mode_controller_` is destroyed. Note
  // that 'capture' in `CaptureModeController` means 'screenshot capture, while
  // 'capture' in `wm::CaptureController` means 'input capture'. Some windows
  // like popup windows close themselves when losing 'input capture' but if
  // 'screenshot capture' is in progress, they do not close themselves. For this
  // reason, change of 'input capture' can cause a call to
  // `CaptureModeController::Get()`. By calling `PrepareForShutdown()` here, it
  // prevents `CaptureModeController::Get()` from being called after the object
  // is destroyed.
  wm::CaptureController::Get()->PrepareForShutdown();

  // This must be destroyed before deleting all the windows below in
  // `CloseAllRootWindowChildWindows()`, since shutting down the session will
  // need to access those windows and it will be a UAF.
  // https://crbug.com/1350711.
  capture_mode_controller_.reset();

  // Relies on `overview_controller`.
  post_login_glanceables_metrics_reporter_.reset();

  // Has to happen before `~OverviewController` since it's an observer.
  informed_restore_controller_.reset();

  // Has to happen before `~MruWindowTracker` and after
  // `~GameDashboardController`.
  overview_controller_.reset();

  // This must be called before deleting all the windows below in
  // `CloseAllRootWindowChildWindows()` since host_windows(which gets destroyed)
  // are needed for proper deletion of RoundedDisplayProviders.
  window_tree_host_manager_->ShutdownRoundedDisplays();

  // Close all widgets (including the shelf) and destroy all window containers.
  CloseAllRootWindowChildWindows();

  glanceables_controller_.reset();

  tasks_controller_.reset();

  multitask_menu_nudge_delegate_.reset();
  tablet_mode_controller_.reset();
  login_screen_controller_.reset();

  // This must be destroyed before `message_center_controller_` in order to
  // restore the original settings if a focus session was active. Also, this
  // should be destroyed before `system_notification_controller_`, which could
  // be indirectly called by `focus_mode_controller_` to update the DND
  // notification.
  focus_mode_controller_.reset();

  system_notification_controller_.reset();
  // Should be destroyed after Shelf and |system_notification_controller_|.
  system_tray_model_.reset();
  system_sounds_delegate_.reset();

  // MultiDisplayMetricsController has a dependency on `mru_window_tracker_`.
  multi_display_metrics_controller_.reset();

  // MruWindowTracker must be destroyed after all windows have been deleted to
  // avoid a possible crash when Shell is destroyed from a non-normal shutdown
  // path. (crbug.com/485438).
  mru_window_tracker_.reset();

  // These need a valid Shell instance to clean up properly, so explicitly
  // delete them before invalidating the instance.
  // Alphabetical. TODO(oshima): sort.
  mouse_keys_controller_.reset();
  autoclick_controller_.reset();
  fullscreen_magnifier_controller_.reset();
  tooltip_controller_.reset();
  event_client_.reset();
  toplevel_window_event_handler_.reset();
  visibility_controller_.reset();

  tray_action_.reset();

  power_button_controller_.reset();
  lock_state_controller_.reset();
  backlights_forced_off_setter_.reset();

  float_controller_.reset();
  pip_controller_.reset();
  screen_pinning_controller_.reset();

  multidevice_notification_presenter_.reset();
  resolution_notification_controller_.reset();
  mouse_cursor_filter_.reset();
  modality_filter_.reset();

  touch_transformer_controller_.reset();
  key_accessibility_enabler_.reset();

  display_speaker_controller_.reset();
  screen_switch_check_controller_.reset();

  ScreenAsh::CreateScreenForShutdown();
  display_configuration_controller_.reset();

  // Needs to be destructed before `ime_controller_`.
  keyboard_backlight_color_controller_.reset();
  rgb_keyboard_manager_.reset();

  ash_color_provider_.reset();

  // Depends on `dark_light_mode_controller_` and `wallpaper_controller_` so it
  // should be destroyed first.
  color_palette_controller_.reset();

  // Depends on `geolocation_controller_` and `wallpaper_controller_`, so it
  // must be destructed before the geolocation controller and wallpaper
  // controller.
  dark_light_mode_controller_.reset();

  // These members access Shell in their destructors.
  wallpaper_controller_.reset();
  accessibility_controller_.reset();
  accessibility_delegate_.reset();
  accessibility_focus_ring_controller_.reset();
  policy_recommendation_restorer_.reset();
  active_session_auth_controller_.reset();
  ime_controller_.reset();
  back_gesture_event_handler_.reset();

  // Balances the Install() in Initialize().
  views::FocusManagerFactory::Install(nullptr);

  // ShelfWindowWatcher has window observers and a pointer to the shelf model.
  shelf_window_watcher_.reset();

  // Removes itself as an observer of |pref_service_|.
  shelf_controller_.reset();

  // `CameraEffectsController` depends on `AutozoomController`, so it must be
  // destructed before it.
  camera_effects_controller_.reset();

  // NightLightControllerImpl depends on the PrefService, the window tree host
  // manager, and `geolocation_controller_`, so it must be destructed before
  // them. crbug.com/724231.
  night_light_controller_ = nullptr;
  // Similarly for DockedMagnifierController.
  docked_magnifier_controller_ = nullptr;
  // Similarly for PrivacyScreenController.
  privacy_screen_controller_ = nullptr;
  // Similarly for AutozoomControllerImpl
  autozoom_controller_ = nullptr;

  geolocation_controller_.reset();

  // NearbyShareDelegateImpl must be destroyed before SessionController and
  // NearbyShareControllerImpl.
  nearby_share_delegate_.reset();
  nearby_share_controller_.reset();

  // Stop observing window activation changes before closing all windows.
  focus_controller_->RemoveObserver(this);

  // Depends on shelf owned by RootWindowController so destroy this before the
  // |window_tree_host_manager_|.
  clipboard_history_controller_.reset();

  // Should be destroyed after `clipboard_history_controller_` and
  // `autozoom_controller_` since they will destruct `SystemNudgeController`.
  system_nudge_pause_manager_.reset();

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

  tab_cluster_ui_controller_.reset();

  focus_rules_ = nullptr;
  focus_controller_.reset();
  screen_position_controller_.reset();

  display_color_manager_.reset();
  projecting_observer_.reset();

  partial_magnifier_controller_.reset();

  laser_pointer_controller_.reset();

  refresh_rate_controller_.reset();

  display_performance_mode_controller_.reset();

  if (display_change_observer_) {
    display_manager_->configurator()->RemoveObserver(
        display_change_observer_.get());
  }
  if (display_error_observer_) {
    display_manager_->configurator()->RemoveObserver(
        display_error_observer_.get());
  }
  display_change_observer_.reset();
  display_shutdown_observer_.reset();

  display_port_observer_.reset();

  keyboard_controller_.reset();

  // Depends on PowerStatus.
  battery_saver_controller_.reset();

  PowerStatus::Shutdown();
  // Depends on SessionController.
  power_event_observer_.reset();

  session_controller_->RemoveObserver(this);
  // TouchDevicesController depends on the PrefService and must be destructed
  // before it.
  touch_devices_controller_ = nullptr;
  // DetachableBaseNotificationController depends on DetachableBaseHandler, and
  // has to be destructed before it.
  detachable_base_notification_controller_.reset();
  // DetachableBaseHandler depends on the PrefService and must be destructed
  // before it.
  detachable_base_handler_.reset();

  diagnostics_log_controller_.reset();

  firmware_update_notification_controller_.reset();

  firmware_update_manager_.reset();

  pcie_peripheral_notification_controller_.reset();

  usb_peripheral_notification_controller_.reset();

  keyboard_capability_.reset();

  message_center_ash_impl_.reset();

  // Destroys the MessageCenter singleton, so must happen late.
  message_center_controller_.reset();

  // `HoldingSpaceController` observes `SessionController` and must be
  // destructed before it.
  holding_space_controller_.reset();

  // `CalendarController` observes `SessionController` and must be destructed
  // before it.
  calendar_controller_.reset();

  audio_effects_controller_.reset();

  shell_delegate_.reset();

  multi_capture_service_client_.reset();

  // Observes `SessionController` and must be destroyed before it.
  federated_service_controller_.reset();
  brightness_control_delegate_.reset();
  keyboard_brightness_control_delegate_.reset();

  UsbguardClient::Shutdown();

  // Must be shut down after detachable_base_handler_.
  HammerdClient::Shutdown();

  if (FwupdClient::Get()) {
    FwupdClient::Shutdown();
  }

  for (auto& observer : shell_observers_) {
    observer.OnShellDestroyed();
  }

  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void Shell::Init(
    ui::ContextFactory* context_factory,
    PrefService* local_state,
    std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory,
    std::unique_ptr<ash::quick_pair::Mediator::Factory>
        quick_pair_mediator_factory,
    scoped_refptr<dbus::Bus> dbus_bus,
    std::unique_ptr<display::NativeDisplayDelegate> native_display_delegate) {
  native_display_delegate_ = std::move(native_display_delegate);
  login_unlock_throughput_recorder_ =
      std::make_unique<LoginUnlockThroughputRecorder>();
  unlock_throughput_recorder_ = std::make_unique<UnlockThroughputRecorder>();

  // Required by DetachableBaseHandler.
  chromeos::InitializeDBusClient<HammerdClient>(dbus_bus.get());

  chromeos::InitializeDBusClient<UsbguardClient>(dbus_bus.get());

  local_state_ = local_state;

  if (features::IsBatterySaverAvailable()) {
    battery_saver_controller_ =
        std::make_unique<BatterySaverController>(local_state_);
  } else {
    // It's possible that we have a new Chrome without battery saver mode
    // available, but still have battery saver running because the previous
    // chrome did. So unconditionally reset battery saver state.
    BatterySaverController::ResetState(local_state_);
  }

  // This creates the MessageCenter object which is used by some other objects
  // initialized here, so it needs to come early.
  message_center_controller_ = std::make_unique<MessageCenterController>();

  message_center_ash_impl_ = std::make_unique<MessageCenterAshImpl>();

  // Initialized early since it is used by some other objects.
  keyboard_capability_ = std::make_unique<ui::KeyboardCapability>();

  // This needs to be initialized after SessionController.
  brightness_control_delegate_ =
      std::make_unique<system::BrightnessControllerChromeos>(
          local_state_, session_controller_.get());

  // This needs to be initialized after SessionController.
  keyboard_brightness_control_delegate_ =
      std::make_unique<KeyboardBrightnessController>(local_state_,
                                                     session_controller_.get());

  // These controllers call Shell::Get() in their constructors, so they cannot
  // be in the member initialization list.
  touch_devices_controller_ = std::make_unique<TouchDevicesController>();
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
  if (features::IsCrossDeviceFeatureSuiteAllowed()) {
    multidevice_notification_presenter_ =
        std::make_unique<MultiDeviceNotificationPresenter>(
            message_center::MessageCenter::Get());
  }
  media_controller_ = std::make_unique<MediaControllerImpl>();
  media_notification_provider_ =
      shell_delegate_->CreateMediaNotificationProvider();

  tablet_mode_controller_ = std::make_unique<TabletModeController>();

  rgb_keyboard_manager_ =
      std::make_unique<RgbKeyboardManager>(ime_controller_.get());

  // Observes the tablet mode controller if any hps feature is enabled.
  if (features::IsSnoopingProtectionEnabled() ||
      features::IsQuickDimEnabled()) {
    human_presence_orientation_controller_ =
        std::make_unique<HumanPresenceOrientationController>();
  }

  // Construct SnoopingProtectionController, must be constructed after
  // HumanPresenceOrientationController.
  if (features::IsSnoopingProtectionEnabled()) {
    snooping_protection_controller_ =
        std::make_unique<SnoopingProtectionController>();
  }

  // Manages lifetime of DiagnosticApp logs.
  diagnostics_log_controller_ =
      std::make_unique<diagnostics::DiagnosticsLogController>();

  pcie_peripheral_notification_controller_ =
      std::make_unique<PciePeripheralNotificationController>(
          message_center::MessageCenter::Get());

  usb_peripheral_notification_controller_ =
      std::make_unique<UsbPeripheralNotificationController>(
          message_center::MessageCenter::Get());

  accessibility_focus_ring_controller_ =
      std::make_unique<AccessibilityFocusRingControllerImpl>();
  accessibility_delegate_.reset(shell_delegate_->CreateAccessibilityDelegate());
  accessibility_controller_ = std::make_unique<AccessibilityController>();
  toast_manager_ = std::make_unique<ToastManagerImpl>();
  anchored_nudge_manager_ = std::make_unique<AnchoredNudgeManagerImpl>();
  system_nudge_pause_manager_ = std::make_unique<SystemNudgePauseManagerImpl>();

  peripheral_battery_listener_ = std::make_unique<PeripheralBatteryListener>();

  peripheral_battery_notifier_ = std::make_unique<PeripheralBatteryNotifier>(
      peripheral_battery_listener_.get());
  power_event_observer_ = std::make_unique<PowerEventObserver>();
  window_cycle_controller_ = std::make_unique<WindowCycleController>();

  capture_mode_controller_ = std::make_unique<CaptureModeController>(
      shell_delegate_->CreateCaptureModeDelegate());

  // Accelerometer file reader starts listening to tablet mode controller.
  AccelerometerReader::GetInstance()->StartListenToTabletModeController();

  // Install the custom factory early on so that views::FocusManagers for Tray,
  // Shelf, and WallPaper could be created by the factory.
  views::FocusManagerFactory::Install(new AshFocusManagerFactory);

  wallpaper_controller_ = WallpaperControllerImpl::Create(local_state_);

  // Initialized after |rgb_keyboard_manager_| to observe the state of rgb
  // keyboard and |wallpaper_controller_| because we will need to observe when
  // the extracted wallpaper color changes.
  keyboard_backlight_color_controller_ =
      std::make_unique<KeyboardBacklightColorController>(local_state_);

  native_cursor_manager_ = new NativeCursorManagerAsh;
  cursor_manager_ = std::make_unique<CursorManager>(
      base::WrapUnique(native_cursor_manager_.get()));

  InitializeDisplayManager();

  // RefreshFontParams depends on display prefs.
  display_manager_->RefreshFontParams();

  // This will initialize aura::Env which requires |display_manager_| to
  // be initialized first.
  aura::Env* env = aura::Env::GetInstance();
  if (context_factory) {
    env->set_context_factory(context_factory);
  }

  ash_color_provider_ = std::make_unique<AshColorProvider>();
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(AddCrosStylesColorMixer));
  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(AddAshColorMixer));

  // Geolocation controller needs to be created before any `ScheduledFeature`
  // subclasses such as night light and dark mode controllers because
  // `ScheduledFeature` ctor will access `geolocation_controller_` from
  // `Shell`.
  geolocation_controller_ = std::make_unique<GeolocationController>(
      SimpleGeolocationProvider::GetInstance());

  // Night Light depends on the display manager, the display color manager,
  // aura::Env, and geolocation controller, so initialize it after all have
  // been initialized.
  night_light_controller_ = std::make_unique<NightLightControllerImpl>();

  dark_light_mode_controller_ = std::make_unique<DarkLightModeControllerImpl>();

  color_palette_controller_ =
      ColorPaletteController::Create(dark_light_mode_controller_.get(),
                                     wallpaper_controller_.get(), local_state_);

  // Privacy Screen depends on the display manager, so initialize it after
  // display manager was properly initialized.
  privacy_screen_controller_ = std::make_unique<PrivacyScreenController>();

  if (media::ShouldEnableAutoFraming()) {
    autozoom_controller_ = std::make_unique<AutozoomControllerImpl>();
  }

  // Fast Pair depends on the display manager, so initialize it after
  // display manager was properly initialized.
  if (base::FeatureList::IsEnabled(features::kFastPair) &&
      quick_pair_mediator_factory) {
    quick_pair_mediator_ = quick_pair_mediator_factory->BuildInstance();
  }

  // The WindowModalityController needs to be at the front of the input event
  // pretarget handler list to ensure that it processes input events when modal
  // windows are active.
  window_modality_controller_ =
      std::make_unique<::wm::WindowModalityController>(this, env);

  input_device_key_alias_manager_ =
      std::make_unique<InputDeviceKeyAliasManager>();

  // The `InputDeviceSettingsController` is a dependency of the following so it
  // must be initialized first:
  //  - `EventRewriterController`
  //  - `InputDeviceTracker`
  //  - `KeyboardModifierMetricsRecorder`
  //  - `InputDeviceSettingsDispatcher`
  input_device_settings_controller_ =
      std::make_unique<InputDeviceSettingsControllerImpl>(local_state_);
  accessibility_controller_->ObserveInputDeviceSettings();
  input_device_tracker_ = std::make_unique<InputDeviceTracker>();
  input_device_settings_dispatcher_ =
      std::make_unique<InputDeviceSettingsDispatcher>(
          ui::OzonePlatform::GetInstance()->GetInputController());
  keyboard_modifier_metrics_recorder_ =
      std::make_unique<KeyboardModifierMetricsRecorder>();
  touchscreen_metrics_recorder_ =
      std::make_unique<TouchscreenMetricsRecorder>();
  event_rewriter_controller_ = std::make_unique<EventRewriterControllerImpl>();
  modifier_key_combo_recorder_ = std::make_unique<ModifierKeyComboRecorder>();
  rapid_key_sequence_recorder_ = std::make_unique<RapidKeySequenceRecorder>();

  env_filter_ = std::make_unique<::wm::CompoundEventFilter>();
  AddPreTargetHandler(env_filter_.get());

  // FocusController takes ownership of AshFocusRules.
  focus_rules_ = new AshFocusRules();
  focus_controller_ = std::make_unique<::wm::FocusController>(focus_rules_);
  focus_controller_->AddObserver(this);

  overview_controller_ = std::make_unique<OverviewController>();

  // `GameDashboardController` has dependencies on `OverviewController` and
  // `CaptureModeController`.
  if (features::IsGameDashboardEnabled()) {
    game_dashboard_controller_ = std::make_unique<GameDashboardController>(
        shell_delegate_->CreateGameDashboardDelegate());
  }

  // `SnapGroupController` has dependencies on `OverviewController` and
  // `TabletModeController`.
  snap_group_controller_ = std::make_unique<SnapGroupController>();

  screen_position_controller_ = std::make_unique<ScreenPositionController>();

  frame_throttling_controller_ = std::make_unique<FrameThrottlingController>(
      context_factory->GetHostFrameSinkManager());

  if (features::IsBirchCoralEnabled()) {
    tab_cluster_ui_controller_ = std::make_unique<TabClusterUIController>();
  }

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
  saved_desk_delegate_ = shell_delegate_->CreateSavedDeskDelegate();
  // Initialized here since it depends on desks.
  saved_desk_controller_ = std::make_unique<SavedDeskController>();

  Shell::SetRootWindowForNewWindows(GetPrimaryRootWindow());

  resolution_notification_controller_ =
      std::make_unique<ResolutionNotificationController>();

  cursor_manager_->SetDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay());

  // Initialize before AcceleratorController and AshAcceleratorConfiguration.
  accelerator_prefs_ = std::make_unique<AcceleratorPrefs>(
      shell_delegate_->CreateAcceleratorPrefsDelegate());

  // Must be initialized after InputMethodManager.
  accelerator_keycode_lookup_cache_ =
      std::make_unique<AcceleratorKeycodeLookupCache>();
  ash_accelerator_configuration_ =
      std::make_unique<AshAcceleratorConfiguration>();
  ash_accelerator_configuration_->Initialize();
  accelerator_lookup_ =
      std::make_unique<AcceleratorLookup>(ash_accelerator_configuration_.get());
  accelerator_controller_ = std::make_unique<AcceleratorControllerImpl>(
      ash_accelerator_configuration_.get());

  clipboard_history_controller_ =
      std::make_unique<ClipboardHistoryControllerImpl>(
          shell_delegate_->CreateClipboardHistoryControllerDelegate());

  // `HoldingSpaceController` must be instantiated before the shelf.
  holding_space_controller_ = std::make_unique<HoldingSpaceController>();

  calendar_controller_ = std::make_unique<CalendarController>();

  if (features::IsVideoConferenceEnabled()) {
    camera_effects_controller_ = std::make_unique<CameraEffectsController>();
    audio_effects_controller_ = std::make_unique<AudioEffectsController>();
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

  control_v_histogram_recorder_ = std::make_unique<ControlVHistogramRecorder>();
  AddPreTargetHandler(control_v_histogram_recorder_.get(),
                      ui::EventTarget::Priority::kAccessibility);

  // AcceleratorTracker should be placed before AcceleratorFilter to make sure
  // the accelerators won't be filtered out before getting AcceleratorTracker.
  accelerator_tracker_ = std::make_unique<AcceleratorTracker>(
      base::make_span(kAcceleratorTrackerList, kAcceleratorTrackerListLength));
  AddPreTargetHandler(accelerator_tracker_.get());

  accelerator_filter_ = std::make_unique<::wm::AcceleratorFilter>(
      std::make_unique<PreTargetAcceleratorHandler>());
  AddPreTargetHandler(accelerator_filter_.get());

  event_transformation_handler_ =
      std::make_unique<EventTransformationHandler>();
  AddPreTargetHandler(event_transformation_handler_.get());

  back_gesture_event_handler_ = std::make_unique<BackGestureEventHandler>();
  AddPreTargetHandler(back_gesture_event_handler_.get());

  toplevel_window_event_handler_ =
      std::make_unique<ToplevelWindowEventHandler>();

  system_gesture_filter_ = std::make_unique<SystemGestureEventFilter>();
  AddPreTargetHandler(system_gesture_filter_.get());

  sticky_keys_controller_ = std::make_unique<StickyKeysController>();
  screen_pinning_controller_ = std::make_unique<ScreenPinningController>();

  power_prefs_ = std::make_unique<PowerPrefs>(
      chromeos::PowerPolicyController::Get(),
      chromeos::PowerManagerClient::Get(), local_state_);

  backlights_forced_off_setter_ = std::make_unique<BacklightsForcedOffSetter>();

  tray_action_ =
      std::make_unique<TrayAction>(backlights_forced_off_setter_.get());

  lock_state_controller_ = std::make_unique<LockStateController>(
      shutdown_controller_.get(), local_state_);
  power_button_controller_ = std::make_unique<PowerButtonController>(
      backlights_forced_off_setter_.get());
  // Pass the initial display state to PowerButtonController.
  power_button_controller_->OnDisplayConfigurationChanged(
      display_configurator()->cached_displays());

  drag_drop_controller_ = std::make_unique<DragDropController>();

  mouse_cursor_filter_ = std::make_unique<MouseCursorEventFilter>();
  AddAccessibilityEventHandler(
      mouse_cursor_filter_.get(),
      AccessibilityEventHandlerManager::HandlerType::kCursor);

  if (features::IsAdaptiveChargingEnabled()) {
    adaptive_charging_controller_ =
        std::make_unique<AdaptiveChargingController>();
  }

  // Create Controllers that may need root window.
  // TODO(oshima): Move as many controllers before creating
  // RootWindowController as possible.
  visibility_controller_ = std::make_unique<AshVisibilityController>();

  laser_pointer_controller_ = std::make_unique<LaserPointerController>();
  partial_magnifier_controller_ =
      std::make_unique<PartialMagnifierController>();

  fullscreen_magnifier_controller_ =
      std::make_unique<FullscreenMagnifierController>();
  mru_window_tracker_ = std::make_unique<MruWindowTracker>();
  assistant_controller_ = std::make_unique<AssistantControllerImpl>();

  // MultiDisplayMetricsController has a dependency on `mru_window_tracker_`.
  multi_display_metrics_controller_ =
      std::make_unique<MultiDisplayMetricsController>();

  mojo::PendingRemote<device::mojom::Fingerprint> ambient_fingerprint;
  shell_delegate_->BindFingerprint(
      ambient_fingerprint.InitWithNewPipeAndPassReceiver());
  ambient_controller_ =
      std::make_unique<AmbientController>(std::move(ambient_fingerprint));

  mojo::PendingRemote<video_capture::mojom::MultiCaptureService>
      multi_capture_service;
  shell_delegate_->BindMultiCaptureService(
      multi_capture_service.InitWithNewPipeAndPassReceiver());
  multi_capture_service_client_ = std::make_unique<MultiCaptureServiceClient>(
      std::move(multi_capture_service));

  // |tablet_mode_controller_| |mru_window_tracker_|, and
  // |assistant_controller_| are put before |app_list_controller_| as they are
  // used in its constructor.
  app_list_controller_ = std::make_unique<AppListControllerImpl>();

  if (features::IsForestFeatureEnabled()) {
    birch_model_ = std::make_unique<BirchModel>();
    birch_privacy_nudge_controller_ =
        std::make_unique<BirchPrivacyNudgeController>();
  }

  autoclick_controller_ = std::make_unique<AutoclickController>();

  if (::features::IsAccessibilityMouseKeysEnabled()) {
    mouse_keys_controller_ = std::make_unique<MouseKeysController>();
    AddPreTargetHandler(mouse_keys_controller_.get());
  }

  color_enhancement_controller_ =
      std::make_unique<ColorEnhancementController>();

  security_curtain_controller_ =
      std::make_unique<curtain::SecurityCurtainControllerImpl>(this);

  docked_magnifier_controller_ = std::make_unique<DockedMagnifierController>();

  video_detector_ = std::make_unique<VideoDetector>();

  auto tooltip_aura = std::make_unique<views::corewm::TooltipAura>(
      base::BindRepeating(&StyleUtil::CreateAshStyleTooltipView));
  tooltip_aura->SetMaxWidth(kTooltipMaxWidth);
  tooltip_controller_ = std::make_unique<views::corewm::TooltipController>(
      std::move(tooltip_aura), activation_client());
  AddPreTargetHandler(tooltip_controller_.get());

  modality_filter_ = std::make_unique<SystemModalContainerEventFilter>(this);
  AddPreTargetHandler(modality_filter_.get());

  if (features::IsPeripheralCustomizationEnabled() ||
      ::features::IsShortcutCustomizationEnabled()) {
    shortcut_input_handler_ = std::make_unique<ShortcutInputHandler>();
    AddPreTargetHandler(shortcut_input_handler_.get());
  }

  event_client_ = std::make_unique<EventClientImpl>();

  resize_shadow_controller_ = std::make_unique<ResizeShadowController>();
  raster_scale_controller_ = std::make_unique<RasterScaleController>();
  shadow_controller_ = std::make_unique<::wm::ShadowController>(
      focus_controller_.get(), std::make_unique<WmShadowControllerDelegate>(),
      env);

  if (features::IsFocusModeEnabled()) {
    tasks_controller_ = std::make_unique<api::TasksController>(
        shell_delegate_->CreateTasksDelegate());
    focus_mode_controller_ = std::make_unique<FocusModeController>(
        shell_delegate_->CreateFocusModeDelegate());
  }

  logout_confirmation_controller_ =
      std::make_unique<LogoutConfirmationController>();

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

  // The `shelf_controller_` needs `app_list_controller_` to initialize
  // launcher_nudge_controller_.
  shelf_controller_->Init();

  nearby_share_controller_ = std::make_unique<NearbyShareControllerImpl>();
  nearby_share_delegate_ = shell_delegate_->CreateNearbyShareDelegate(
      nearby_share_controller_.get());

  // System sounds delegate should be initialized before
  // `SystemNotificationController` is created, because
  // `SystemNotificationController` ctor will creat an instance of
  // `PowerSoundsController`, which will access and play the initialized sounds.
  system_sounds_delegate_ = shell_delegate_->CreateSystemSoundsDelegate();
  system_sounds_delegate_->Init();

  privacy_hub_controller_ = PrivacyHubController::CreatePrivacyHubController();

  // One of the subcontrollers accesses the SystemNotificationController.
  system_notification_controller_ =
      std::make_unique<SystemNotificationController>();

  // WmModeController should be created before initializing the window tree
  // hosts, since the latter will initialize the shelf on each display, which
  // hosts the WM mode tray button.
  if (features::IsWmModeEnabled()) {
    wm_mode_controller_ = std::make_unique<WmModeController>();
  }

  hotspot_icon_animation_ = std::make_unique<HotspotIconAnimation>();
  hotspot_info_cache_ = std::make_unique<HotspotInfoCache>();

  window_tree_host_manager_->InitHosts();
  display_manager_->NotifyDisplaysInitialized();

  if (ash::features::IsBootAnimationEnabled()) {
    booting_animation_controller_ =
        std::make_unique<BootingAnimationController>();
  }

  // Create virtual keyboard after WindowTreeHostManager::InitHosts() since it
  // may enable the virtual keyboard immediately, which requires a
  // WindowTreeHostManager to host the keyboard window.
  keyboard_controller_->CreateVirtualKeyboard(std::move(keyboard_ui_factory));

  // Create window restore controller after `WindowTreeHostManager::InitHosts()`
  // since it may need to add observers to root windows.
  window_restore_controller_ = std::make_unique<WindowRestoreController>();

  static_cast<CursorManager*>(cursor_manager_.get())->Init();

  mojo::PendingRemote<device::mojom::Fingerprint> fingerprint;
  shell_delegate_->BindFingerprint(
      fingerprint.InitWithNewPipeAndPassReceiver());
  user_activity_notifier_ =
      std::make_unique<ui::UserActivityPowerManagerNotifier>(
          ui::UserActivityDetector::Get(), std::move(fingerprint));
  video_activity_notifier_ =
      std::make_unique<VideoActivityNotifier>(video_detector_.get());
  bluetooth_state_cache_ = std::make_unique<BluetoothStateCache>();
  bluetooth_device_status_ui_handler_ =
      std::make_unique<BluetoothDeviceStatusUiHandler>(local_state_);
  bluetooth_notification_controller_ =
      std::make_unique<BluetoothNotificationController>(
          message_center::MessageCenter::Get());
  screen_orientation_controller_ =
      std::make_unique<ScreenOrientationController>();

  cros_display_config_ = std::make_unique<CrosDisplayConfig>();

  screen_layout_observer_ = std::make_unique<ScreenLayoutObserver>();
  sms_observer_ = std::make_unique<SmsObserver>();
  snap_controller_ = std::make_unique<SnapControllerImpl>();
  key_accessibility_enabler_ = std::make_unique<KeyAccessibilityEnabler>();

  // The compositor thread and main message loop have to be running in
  // order to create mirror window. Run it after the main message loop
  // is started.
  display_manager_->CreateMirrorWindowAsyncIfAny();

  // TODO(1091497): Consider combining DisplayHighlightController and
  // DisplayAlignmentController.
  display_highlight_controller_ =
      std::make_unique<DisplayHighlightController>();

  if (features::IsDisplayAlignmentAssistanceEnabled()) {
    display_alignment_controller_ =
        std::make_unique<DisplayAlignmentController>();
  }

  if (features::AreAnyGlanceablesTimeManagementViewsEnabled()) {
    glanceables_controller_ = std::make_unique<GlanceablesController>();
  }
  post_login_glanceables_metrics_reporter_ =
      std::make_unique<PostLoginGlanceablesMetricsRecorder>();

  projector_controller_ = std::make_unique<ProjectorControllerImpl>();
  annotator_controller_ = std::make_unique<AnnotatorController>();

  float_controller_ = std::make_unique<FloatController>();
  if (features::IsForestFeatureEnabled()) {
    informed_restore_controller_ =
        std::make_unique<InformedRestoreController>();
  }
  pip_controller_ = std::make_unique<PipController>();

  multitask_menu_nudge_delegate_ =
      std::make_unique<MultitaskMenuNudgeDelegateAsh>();

  if (features::IsFederatedServiceEnabled()) {
    federated_service_controller_ =
        std::make_unique<federated::FederatedServiceControllerImpl>();
  }

  if (features::IsUserEducationEnabled()) {
    user_education_controller_ = std::make_unique<UserEducationController>(
        shell_delegate_->CreateUserEducationDelegate());
  }

  if (features::IsCoralFeatureEnabled()) {
    coral_controller_ = std::make_unique<CoralController>();
  }
  if (features::IsBirchCoralEnabled()) {
    coral_delegate_ = shell_delegate_->CreateCoralDelegate();
  }

  if (features::IsPickerUpdateEnabled()) {
    picker_controller_ = std::make_unique<PickerController>();
  }

  if (features::IsLobsterEnabled() && LobsterController::IsEnabled()) {
    lobster_controller_ = std::make_unique<LobsterController>();
  }

  if (features::IsScannerEnabled() && ScannerController::IsEnabled()) {
    scanner_controller_ = std::make_unique<ScannerController>(
        shell_delegate_->CreateScannerDelegate());
  }

  if (features::IsTilingWindowResizeEnabled()) {
    window_tiling_controller_ = std::make_unique<WindowTilingController>();
  }

  // Injects the factory which fulfills the implementation of the text context
  // menu exclusive to CrOS.
  views::ViewsTextServicesContextMenuChromeos::SetImplFactory(
      base::BindRepeating(
          [](ui::SimpleMenuModel* menu_model, views::Textfield* textfield)
              -> std::unique_ptr<views::ViewsTextServicesContextMenu> {
            return std::make_unique<ViewsTextServicesContextMenuAsh>(menu_model,
                                                                     textfield);
          }));

  // Sets the implementation of clipboard history utility functions.
  // It is safe to pass `clipboard_history_controller_` raw pointer here.
  // Because the function implementation is reset before
  // `clipboard_history_controller_` is destroyed.
  chromeos::clipboard_history::SetQueryItemDescriptorsImpl(base::BindRepeating(
      [](ClipboardHistoryControllerImpl* controller) {
        std::vector<crosapi::mojom::ClipboardHistoryItemDescriptor> descriptors;
        if (clipboard_history_util::IsEnabledInCurrentMode()) {
          const auto& items = controller->history()->GetItems();
          descriptors.reserve(items.size());
          base::ranges::transform(items, std::back_inserter(descriptors),
                                  &clipboard_history_util::ItemToDescriptor);
        }
        return descriptors;
      },
      clipboard_history_controller_.get()));
  chromeos::clipboard_history::SetPasteClipboardItemByIdImpl(
      base::BindRepeating(
          [](const base::UnguessableToken& id, int event_flags,
             crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
            ClipboardHistoryController::Get()->PasteClipboardItemById(
                id.ToString(), event_flags, show_source);
          }));

  for (auto& observer : shell_observers_) {
    observer.OnShellInitialized();
  }

  user_metrics_recorder_->OnShellInitialized();

  occlusion_tracker_pauser_ = std::make_unique<OcclusionTrackerPauser>();

  // Initialize the D-Bus bus and services for ash.
  dbus_bus_ = dbus_bus;
  ash_dbus_services_ = std::make_unique<AshDBusServices>(dbus_bus.get());

  tab_strip_delegate_ = shell_delegate_->CreateTabStripDelegate();
}

void Shell::InitializeDisplayManager() {
  if (!native_display_delegate_) {
    native_display_delegate_ =
        ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
  }
  display_manager_->InitConfigurator(std::move(native_display_delegate_));
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
    if (base::SysInfo::IsRunningOnChromeOS()) {
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

  display_color_manager_ =
      std::make_unique<DisplayColorManager>(display_manager_->configurator());

  display_port_observer_ = std::make_unique<display::DisplayPortObserver>(
      display_manager_->configurator(),
      base::BindRepeating([](const std::vector<uint32_t>& port_nums) {
        TypecdClient::Get()->SetTypeCPortsUsingDisplays(port_nums);
      }));

  if (!display_initialized) {
    display_manager_->InitDefaultDisplay();
  }

  display_performance_mode_controller_ =
      std::make_unique<DisplayPerformanceModeController>();

  refresh_rate_controller_ = std::make_unique<RefreshRateController>(
      display_configurator(), PowerStatus::Get(),
      display_performance_mode_controller());
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
    if (controller->CanWindowReceiveEvents(window)) {
      return true;
    }
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
  return nullptr;
}

ui::EventTargeter* Shell::GetEventTargeter() {
  NOTREACHED();
}

void Shell::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active) {
    return;
  }

  Shell::SetRootWindowForNewWindows(gained_active->GetRootWindow());
}

void Shell::OnFirstSessionStarted() {
  // Enable magnifier scroll keys as there may be no mouse cursor in kiosk mode.
  MagnifierKeyScroller::SetEnabled(session_controller_->IsRunningInAppMode());

  // Enable long press action to toggle spoken feedback with hotrod remote
  // which can't handle shortcuts.
  SpokenFeedbackToggler::SetEnabled(session_controller_->IsRunningInAppMode());

  // Reset user prefs related to contextual tooltips.
  if (switches::ContextualNudgesResetShownCount()) {
    contextual_tooltip::ClearPrefs();
  }

  // The launcher is not available before login, so start tracking usage after
  // the session starts.
  app_list_feature_usage_metrics_ =
      std::make_unique<AppListFeatureUsageMetrics>();
}

void Shell::OnFirstSessionReady() {
  // Initialize the fwupd (firmware updater) DBus client only when the user
  // session is active. Since the fwupd service is only relevant during an
  // active user session, this prevents a bug in which the service would start
  // up earlier than expected and causes a delay during boot.
  // See b/250002264 for more details.
  if (!FwupdClient::Get() && !firmware_update_notification_controller_ &&
      !features::IsBlockFwupdClientEnabled()) {
    chromeos::InitializeDBusClient<FwupdClient>(dbus_bus_.get());
    firmware_update_manager_ = std::make_unique<FirmwareUpdateManager>();
    // The notification controller is registered as an observer before
    // requesting updates to allow a notification to be shown if a critical
    // firmware update is found.
    firmware_update_notification_controller_ =
        std::make_unique<FirmwareUpdateNotificationController>(
            message_center::MessageCenter::Get());
    firmware_update_manager_->RequestAllUpdates(
        FirmwareUpdateManager::Source::kStartup);
  }
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
    for (aura::Window* container : containers) {
      DCHECK(container->children().empty());
    }
  }
#endif
}

}  // namespace ash
