// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/accelerators/modifier_key_combo_recorder.h"
#include "ash/accessibility/accessibility_event_handler_manager.h"
#include "ash/ash_export.h"
#include "ash/constants/ash_features.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/metrics/unlock_throughput_recorder.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/system/input_device_settings/touchscreen_metrics_recorder.h"
#include "ash/system/toast/system_nudge_pause_manager_impl.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event_target.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/public/activation_change_observer.h"

class PrefService;

namespace aura {
class RootWindow;
class Window;
}  // namespace aura

namespace chromeos {
class ImmersiveContext;
class SnapController;
}  // namespace chromeos

namespace dbus {
class Bus;
}

namespace display {
class DisplayChangeObserver;
class DisplayConfigurator;
class DisplayManager;
class DisplayPortObserver;
class NativeDisplayDelegate;
}  // namespace display

namespace gfx {
class Point;
}  // namespace gfx

namespace keyboard {
class KeyboardUIFactory;
}

namespace ui {
class ContextFactory;
class KeyboardCapability;
class UserActivityPowerManagerNotifier;
}  // namespace ui

namespace views {
class NonClientFrameView;
class Widget;
namespace corewm {
class TooltipController;
}
}  // namespace views

namespace wm {
class AcceleratorFilter;
class ActivationClient;
class CompoundEventFilter;
class FocusController;
class ShadowController;
class VisibilityController;
class WindowModalityController;
}  // namespace wm

namespace ash {

class AcceleratorControllerImpl;
class AcceleratorKeycodeLookupCache;
class AcceleratorLookup;
class AcceleratorPrefs;
class AcceleratorTracker;
class AccessibilityController;
class AccessibilityDelegate;
class AccessibilityEventHandlerManager;
class AccessibilityFocusRingControllerImpl;
class AdaptiveChargingController;
class AmbientController;
class AnchoredNudgeManagerImpl;
class AnnotatorController;
class AppListControllerImpl;
class AppListFeatureUsageMetrics;
class AshAcceleratorConfiguration;
class AshColorProvider;
class AshDBusServices;
class AshFocusRules;
class AshTouchTransformController;
class AssistantControllerImpl;
class AudioEffectsController;
class AutoclickController;
class AutozoomControllerImpl;
class BackGestureEventHandler;
class BacklightsForcedOffSetter;
class BatterySaverController;
class BirchModel;
class BirchPrivacyNudgeController;
class BluetoothDeviceStatusUiHandler;
class BluetoothNotificationController;
class BluetoothStateCache;
class BootingAnimationController;
class BrightnessControlDelegate;
class CalendarController;
class CameraEffectsController;
class CaptureModeController;
class ColorPaletteController;
class ControlVHistogramRecorder;
class CoralController;
class CoralDelegate;
class CrosDisplayConfig;
class DarkLightModeControllerImpl;
class DeskProfilesDelegate;
class DesksController;
class DetachableBaseHandler;
class DetachableBaseNotificationController;
class DisplayAlignmentController;
class DisplayColorManager;
class NativeCursorManagerAsh;
class DisplayConfigurationController;
class DisplayConfigurationObserver;
class DisplayErrorObserver;
class DisplayHighlightController;
class DisplayPerformanceModeController;
class DisplayPrefs;
class DisplayShutdownObserver;
class DisplaySpeakerController;
class DockedMagnifierController;
class DragDropController;
class EventClientImpl;
class EventRewriterControllerImpl;
class EventTransformationHandler;
class FirmwareUpdateManager;
class FirmwareUpdateNotificationController;
class FloatController;
class FocusCycler;
class FocusModeController;
class FrameThrottlingController;
class FullscreenMagnifierController;
class GameDashboardController;
class GeolocationController;
class GlanceablesController;
class ColorEnhancementController;
class HoldingSpaceController;
class HotspotIconAnimation;
class HotspotInfoCache;
class HumanPresenceOrientationController;
class ImeControllerImpl;
class InformedRestoreController;
class InputDeviceKeyAliasManager;
class InputDeviceSettingsControllerImpl;
class InputDeviceSettingsDispatcher;
class InputDeviceTracker;
class InSessionAuthDialogControllerImpl;
class WebAuthNDialogController;
class WebAuthNDialogControllerImpl;
class KeyAccessibilityEnabler;
class KeyboardBacklightColorController;
class KeyboardBrightnessControlDelegate;
class KeyboardControllerImpl;
class KeyboardModifierMetricsRecorder;
class LaserPointerController;
class LobsterController;
class LocalAuthenticationRequestController;
class LocaleUpdateControllerImpl;
class LockStateController;
class LoginScreenController;
class LoginUnlockThroughputRecorder;
class LogoutConfirmationController;
class MediaControllerImpl;
class MediaNotificationProvider;
class MessageCenterAshImpl;
class MessageCenterController;
class MouseCursorEventFilter;
class MouseKeysController;
class MruWindowTracker;
class MultiDeviceNotificationPresenter;
class MultiDisplayMetricsController;
class NearbyShareControllerImpl;
class NearbyShareDelegate;
class NightLightControllerImpl;
class OcclusionTrackerPauser;
class OverviewController;
class ParentAccessController;
class PartialMagnifierController;
class PciePeripheralNotificationController;
class PeripheralBatteryListener;
class PeripheralBatteryNotifier;
class PersistentWindowController;
class PickerController;
class PipController;
class PolicyRecommendationRestorer;
class PostLoginGlanceablesMetricsRecorder;
class PowerButtonController;
class PowerEventObserver;
class PowerPrefs;
class PrivacyHubController;
class PrivacyScreenController;
class ProjectingObserver;
class ProjectorControllerImpl;
class RapidKeySequenceRecorder;
class RasterScaleController;
class RefreshRateController;
class ResizeShadowController;
class ResolutionNotificationController;
class RgbKeyboardManager;
class RootWindowController;
class SavedDeskController;
class SavedDeskDelegate;
class TabClusterUIController;
class TabStripDelegate;
class UsbPeripheralNotificationController;
class ScannerController;
class ScreenLayoutObserver;
class ScreenOrientationController;
class ScreenPinningController;
class ScreenPositionController;
class ScreenSwitchCheckController;
class SessionControllerImpl;
class FeatureDiscoveryDurationReporterImpl;
class ShelfConfig;
class ShelfController;
class ShelfWindowWatcher;
class ShellDelegate;
struct ShellInitParams;
class ShellObserver;
class ShortcutInputHandler;
class ShutdownControllerImpl;
class SmsObserver;
class SnapGroupController;
class SnoopingProtectionController;
class StickyKeysController;
class SystemGestureEventFilter;
class SystemModalContainerEventFilter;
class SystemNotificationController;
class SystemNudgePauseManagerImpl;
class SystemSoundsDelegate;
class SystemTrayModel;
class SystemTrayNotifier;
class TabletModeController;
class ToastManagerImpl;
class ToplevelWindowEventHandler;
class ClipboardHistoryControllerImpl;
class TouchDevicesController;
class TrayAction;
class UserEducationController;
class UserMetricsRecorder;
class VideoActivityNotifier;
class VideoDetector;
class WallpaperControllerImpl;
class WindowCycleController;
class WindowRestoreController;
class WindowTilingController;
class WindowTreeHostManager;
class WmModeController;
class ArcInputMethodBoundsTracker;
class MultiCaptureServiceClient;

enum class LoginStatus;

namespace api {
class TasksController;
}  // namespace api

namespace diagnostics {
class DiagnosticsLogController;
}  // namespace diagnostics

namespace federated {
class FederatedServiceControllerImpl;
}  // namespace federated

namespace quick_pair {
class Mediator;
}  // namespace quick_pair

namespace curtain {
class SecurityCurtainController;
}  // namespace curtain

// Shell is a singleton object that presents the Shell API and implements the
// RootWindow's delegate interface.
//
// Upon creation, the Shell sets itself as the RootWindow's delegate, which
// takes ownership of the Shell.
class ASH_EXPORT Shell : public SessionObserver,
                         public SystemModalContainerEventFilterDelegate,
                         public ui::EventTarget,
                         public ::wm::ActivationChangeObserver {
 public:
  typedef std::vector<RootWindowController*> RootWindowControllerList;

  Shell(const Shell&) = delete;
  Shell& operator=(const Shell&) = delete;

  // Creates the single Shell instance.
  static Shell* CreateInstance(ShellInitParams init_params);

  // Should never be called before |CreateInstance()|.
  static Shell* Get();

  // Returns true if the ash shell has been instantiated.
  static bool HasInstance();

  static void DeleteInstance();

  // Returns the root window controller for the primary root window.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowController* GetPrimaryRootWindowController();

  // Returns the RootWindowController for the given display id. If there
  // is no display for |display_id|, null is returned.
  static RootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t display_id);

  // Returns the root Window for the given display id. If there is no display
  // for |display_id| null is returned.
  static aura::Window* GetRootWindowForDisplayId(int64_t display_id);

  // Returns all root window controllers.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowControllerList GetAllRootWindowControllers();

  // Returns the primary root Window. The primary root Window is the one that
  // has a launcher.
  static aura::Window* GetPrimaryRootWindow();

  // Sets the root window that newly created windows should be added to.
  static void SetRootWindowForNewWindows(aura::Window* root);

  // Returns the root window that newly created windows should be added to.
  // Value can be temporarily overridden using
  // display::ScopedDisplayForNewWindows. NOTE: this returns the root, newly
  // created window should be added to the appropriate container in the returned
  // window.
  static aura::Window* GetRootWindowForNewWindows();

  // Returns all root windows.
  static aura::Window::Windows GetAllRootWindows();

  static aura::Window* GetContainer(aura::Window* root_window,
                                    int container_id);
  static const aura::Window* GetContainer(const aura::Window* root_window,
                                          int container_id);

  // If a system-modal dialog window is currently open, returns the ID of the
  // system modal window container that contains the window.
  // If no system-modal dialogs are open it returns -1.
  static int GetOpenSystemModalWindowContainerId();

  // Returns true if a system-modal dialog window is currently open.
  static bool IsSystemModalWindowOpen();

  // Track/Untrack InputMethod bounds.
  void TrackInputMethodBounds(ArcInputMethodBoundsTracker* tracker);
  void UntrackTrackInputMethodBounds(ArcInputMethodBoundsTracker* tracker);

  // Creates a default views::NonClientFrameView for use by windows in the
  // Ash environment.
  std::unique_ptr<views::NonClientFrameView> CreateDefaultNonClientFrameView(
      views::Widget* widget);

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // Called when a root window is created.
  void OnRootWindowAdded(aura::Window* root_window);

  // Called when a root window is about to shutdown.
  void OnRootWindowWillShutdown(aura::Window* root_window);

  // Called when dictation is activated.
  void OnDictationStarted();

  // Called when dictation is ended.
  void OnDictationEnded();

  // DEPRECATED. Use display::Screen::GetScreen()->InTabletMode() instead.
  // TODO(crbug.com/40942452): Remove this.
  //
  // Returns whether the device is currently in tablet mode.
  bool IsInTabletMode() const;

  // Tests if TabletModeWindowManager is not enabled, and if
  // TabletModeController is not currently setting a display rotation. Or if
  // the |resolution_notification_controller_| is not showing its confirmation
  // dialog. If true then changes to display settings can be saved.
  bool ShouldSaveDisplaySettings();

  AcceleratorControllerImpl* accelerator_controller() {
    return accelerator_controller_.get();
  }
  wm::AcceleratorFilter* accelerator_filter() {
    return accelerator_filter_.get();
  }
  AcceleratorPrefs* accelerator_prefs() { return accelerator_prefs_.get(); }
  AcceleratorTracker* accelerator_tracker() {
    return accelerator_tracker_.get();
  }
  AccessibilityController* accessibility_controller() {
    return accessibility_controller_.get();
  }
  AccessibilityDelegate* accessibility_delegate() {
    return accessibility_delegate_.get();
  }
  AccessibilityFocusRingControllerImpl* accessibility_focus_ring_controller() {
    return accessibility_focus_ring_controller_.get();
  }
  ::wm::ActivationClient* activation_client();
  AppListControllerImpl* app_list_controller() {
    return app_list_controller_.get();
  }
  AdaptiveChargingController* adaptive_charging_controller() {
    return adaptive_charging_controller_.get();
  }
  AmbientController* ambient_controller() { return ambient_controller_.get(); }
  AnchoredNudgeManagerImpl* anchored_nudge_manager() {
    return anchored_nudge_manager_.get();
  }
  AshAcceleratorConfiguration* ash_accelerator_configuration() {
    return ash_accelerator_configuration_.get();
  }
  AcceleratorLookup* accelerator_lookup() { return accelerator_lookup_.get(); }
  AssistantControllerImpl* assistant_controller() {
    return assistant_controller_.get();
  }
  AudioEffectsController* audio_effects_controller() {
    return audio_effects_controller_.get();
  }
  AutoclickController* autoclick_controller() {
    return autoclick_controller_.get();
  }
  AutozoomControllerImpl* autozoom_controller() {
    return autozoom_controller_.get();
  }
  BacklightsForcedOffSetter* backlights_forced_off_setter() {
    return backlights_forced_off_setter_.get();
  }
  BatterySaverController* battery_saver_controller() {
    return battery_saver_controller_.get();
  }
  BirchModel* birch_model() { return birch_model_.get(); }
  BirchPrivacyNudgeController* birch_privacy_nudge_controller() {
    return birch_privacy_nudge_controller_.get();
  }
  BluetoothStateCache* bluetooth_state_cache() {
    return bluetooth_state_cache_.get();
  }
  BootingAnimationController* booting_animation_controller() {
    return booting_animation_controller_.get();
  }
  BrightnessControlDelegate* brightness_control_delegate() {
    return brightness_control_delegate_.get();
  }
  CalendarController* calendar_controller() {
    return calendar_controller_.get();
  }
  CameraEffectsController* camera_effects_controller() {
    return camera_effects_controller_.get();
  }
  ColorPaletteController* color_palette_controller() {
    return color_palette_controller_.get();
  }
  CrosDisplayConfig* cros_display_config() {
    return cros_display_config_.get();
  }
  ::wm::CursorManager* cursor_manager() { return cursor_manager_.get(); }
  curtain::SecurityCurtainController& security_curtain_controller() {
    return *security_curtain_controller_;
  }
  DarkLightModeControllerImpl* dark_light_mode_controller() {
    return dark_light_mode_controller_.get();
  }
  DesksController* desks_controller() { return desks_controller_.get(); }
  SavedDeskController* saved_desk_controller() {
    return saved_desk_controller_.get();
  }
  SavedDeskDelegate* saved_desk_delegate() {
    return saved_desk_delegate_.get();
  }
  DetachableBaseHandler* detachable_base_handler() {
    return detachable_base_handler_.get();
  }

  display::DisplayManager* display_manager() { return display_manager_.get(); }
  DisplayPrefs* display_prefs() { return display_prefs_.get(); }
  DisplayConfigurationController* display_configuration_controller() {
    return display_configuration_controller_.get();
  }

  DisplayAlignmentController* display_alignment_controller() {
    return display_alignment_controller_.get();
  }

  display::DisplayConfigurator* display_configurator();

  RefreshRateController* refresh_rate_controller() {
    return refresh_rate_controller_.get();
  }

  DisplayColorManager* display_color_manager() {
    return display_color_manager_.get();
  }
  DisplayErrorObserver* display_error_observer() {
    return display_error_observer_.get();
  }

  ProjectingObserver* projecting_observer() {
    return projecting_observer_.get();
  }

  DisplayHighlightController* display_highlight_controller() {
    return display_highlight_controller_.get();
  }

  DisplayPerformanceModeController* display_performance_mode_controller() {
    return display_performance_mode_controller_.get();
  }

  DockedMagnifierController* docked_magnifier_controller() {
    return docked_magnifier_controller_.get();
  }
  ::wm::CompoundEventFilter* env_filter() { return env_filter_.get(); }
  EventRewriterControllerImpl* event_rewriter_controller() {
    return event_rewriter_controller_.get();
  }

  InputDeviceKeyAliasManager* input_device_key_alias_manager() {
    return input_device_key_alias_manager_.get();
  }

  InputDeviceSettingsControllerImpl* input_device_settings_controller() {
    return input_device_settings_controller_.get();
  }

  InputDeviceTracker* input_device_tracker() {
    return input_device_tracker_.get();
  }

  EventClientImpl* event_client() { return event_client_.get(); }
  EventTransformationHandler* event_transformation_handler() {
    return event_transformation_handler_.get();
  }

  federated::FederatedServiceControllerImpl* federated_service_controller() {
    return federated_service_controller_.get();
  }

  FirmwareUpdateNotificationController*
  firmware_update_notification_controller() {
    return firmware_update_notification_controller_.get();
  }

  FloatController* float_controller() { return float_controller_.get(); }
  ::wm::FocusController* focus_controller() { return focus_controller_.get(); }
  AshFocusRules* focus_rules() { return focus_rules_; }
  FocusCycler* focus_cycler() { return focus_cycler_.get(); }
  FocusModeController* focus_mode_controller() {
    return focus_mode_controller_.get();
  }
  FullscreenMagnifierController* fullscreen_magnifier_controller() {
    return fullscreen_magnifier_controller_.get();
  }
  GameDashboardController* game_dashboard_controller() {
    return game_dashboard_controller_.get();
  }
  GeolocationController* geolocation_controller() {
    return geolocation_controller_.get();
  }
  GlanceablesController* glanceables_controller() {
    return glanceables_controller_.get();
  }
  PostLoginGlanceablesMetricsRecorder*
  post_login_glanceables_metrics_reporter() {
    return post_login_glanceables_metrics_reporter_.get();
  }
  ColorEnhancementController* color_enhancement_controller() {
    return color_enhancement_controller_.get();
  }
  HotspotIconAnimation* hotspot_icon_animation() {
    return hotspot_icon_animation_.get();
  }
  HotspotInfoCache* hotspot_info_cache() { return hotspot_info_cache_.get(); }
  HumanPresenceOrientationController* human_presence_orientation_controller() {
    return human_presence_orientation_controller_.get();
  }
  ImeControllerImpl* ime_controller() { return ime_controller_.get(); }
  WebAuthNDialogController* webauthn_dialog_controller();
  InSessionAuthDialogControllerImpl* in_session_auth_dialog_controller() {
    return in_session_auth_dialog_controller_.get();
  }
  KeyAccessibilityEnabler* key_accessibility_enabler() {
    return key_accessibility_enabler_.get();
  }
  KeyboardBacklightColorController* keyboard_backlight_color_controller() {
    return keyboard_backlight_color_controller_.get();
  }
  KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate() {
    return keyboard_brightness_control_delegate_.get();
  }
  ui::KeyboardCapability* keyboard_capability() {
    return keyboard_capability_.get();
  }
  KeyboardControllerImpl* keyboard_controller() {
    return keyboard_controller_.get();
  }
  KeyboardModifierMetricsRecorder* keyboard_modifier_metrics_recorder() {
    return keyboard_modifier_metrics_recorder_.get();
  }
  TouchscreenMetricsRecorder* touchscreen_metrics_recorder() {
    return touchscreen_metrics_recorder_.get();
  }
  LaserPointerController* laser_pointer_controller() {
    return laser_pointer_controller_.get();
  }
  LobsterController* lobster_controller() { return lobster_controller_.get(); }
  LocaleUpdateControllerImpl* locale_update_controller() {
    return locale_update_controller_.get();
  }
  LocalAuthenticationRequestController*
  local_authentication_request_controller() {
    return local_authentication_request_controller_.get();
  }
  ActiveSessionAuthController* active_session_auth_controller() {
    return active_session_auth_controller_.get();
  }
  LoginScreenController* login_screen_controller() {
    return login_screen_controller_.get();
  }
  LockStateController* lock_state_controller() {
    return lock_state_controller_.get();
  }
  LogoutConfirmationController* logout_confirmation_controller() {
    return logout_confirmation_controller_.get();
  }
  MediaControllerImpl* media_controller() { return media_controller_.get(); }
  MessageCenterAshImpl* message_center_ash_impl() {
    return message_center_ash_impl_.get();
  }
  MessageCenterController* message_center_controller() {
    return message_center_controller_.get();
  }
  MouseCursorEventFilter* mouse_cursor_filter() {
    return mouse_cursor_filter_.get();
  }
  MouseKeysController* mouse_keys_controller() {
    return mouse_keys_controller_.get();
  }
  MruWindowTracker* mru_window_tracker() { return mru_window_tracker_.get(); }
  MultiDeviceNotificationPresenter* multidevice_notification_presenter() {
    return multidevice_notification_presenter_.get();
  }
  MultiDisplayMetricsController* multi_display_metrics_controller() {
    return multi_display_metrics_controller_.get();
  }
  NearbyShareControllerImpl* nearby_share_controller() {
    return nearby_share_controller_.get();
  }
  NearbyShareDelegate* nearby_share_delegate() {
    return nearby_share_delegate_.get();
  }
  NightLightControllerImpl* night_light_controller() {
    return night_light_controller_.get();
  }
  ParentAccessController* parent_access_controller() {
    return parent_access_controller_.get();
  }
  PartialMagnifierController* partial_magnifier_controller() {
    return partial_magnifier_controller_.get();
  }
  PeripheralBatteryListener* peripheral_battery_listener() {
    return peripheral_battery_listener_.get();
  }
  PickerController* picker_controller() { return picker_controller_.get(); }
  InformedRestoreController* informed_restore_controller() {
    return informed_restore_controller_.get();
  }
  PipController* pip_controller() { return pip_controller_.get(); }
  PolicyRecommendationRestorer* policy_recommendation_restorer() {
    return policy_recommendation_restorer_.get();
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }
  PowerEventObserver* power_event_observer() {
    return power_event_observer_.get();
  }
  PrivacyHubController* privacy_hub_controller() {
    return privacy_hub_controller_.get();
  }
  PrivacyScreenController* privacy_screen_controller() {
    return privacy_screen_controller_.get();
  }
  quick_pair::Mediator* quick_pair_mediator() {
    return quick_pair_mediator_.get();
  }
  MultiCaptureServiceClient* multi_capture_service_client() {
    return multi_capture_service_client_.get();
  }
  RasterScaleController* raster_scale_controller() {
    return raster_scale_controller_.get();
  }
  ResizeShadowController* resize_shadow_controller() {
    return resize_shadow_controller_.get();
  }
  ResolutionNotificationController* resolution_notification_controller() {
    return resolution_notification_controller_.get();
  }
  RgbKeyboardManager* rgb_keyboard_manager() {
    return rgb_keyboard_manager_.get();
  }
  ScannerController* scanner_controller() { return scanner_controller_.get(); }
  ScreenLayoutObserver* screen_layout_observer() {
    return screen_layout_observer_.get();
  }
  ScreenOrientationController* screen_orientation_controller() {
    return screen_orientation_controller_.get();
  }
  ScreenPinningController* screen_pinning_controller() {
    return screen_pinning_controller_.get();
  }
  ScreenSwitchCheckController* screen_switch_check_controller() {
    return screen_switch_check_controller_.get();
  }
  SessionControllerImpl* session_controller() {
    return session_controller_.get();
  }
  SnapGroupController* snap_group_controller() {
    return snap_group_controller_.get();
  }
  FeatureDiscoveryDurationReporterImpl* feature_discover_reporter() {
    return feature_discover_reporter_.get();
  }
  ::wm::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }
  ShelfConfig* shelf_config() { return shelf_config_.get(); }
  ShelfController* shelf_controller() { return shelf_controller_.get(); }
  ShellDelegate* shell_delegate() { return shell_delegate_.get(); }
  ShortcutInputHandler* shortcut_input_handler() {
    return shortcut_input_handler_.get();
  }
  ModifierKeyComboRecorder* modifier_key_combo_recorder() {
    return modifier_key_combo_recorder_.get();
  }
  RapidKeySequenceRecorder* rapid_key_sequence_recorder() {
    return rapid_key_sequence_recorder_.get();
  }
  ShutdownControllerImpl* shutdown_controller() {
    return shutdown_controller_.get();
  }
  SnoopingProtectionController* snooping_protection_controller() {
    return snooping_protection_controller_.get();
  }
  StickyKeysController* sticky_keys_controller() {
    return sticky_keys_controller_.get();
  }
  SystemNotificationController* system_notification_controller() {
    return system_notification_controller_.get();
  }
  SystemNudgePauseManagerImpl* system_nudge_pause_manager() {
    return system_nudge_pause_manager_.get();
  }
  SystemTrayModel* system_tray_model() { return system_tray_model_.get(); }
  SystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }
  SystemSoundsDelegate* system_sounds_delegate() {
    return system_sounds_delegate_.get();
  }
  TabClusterUIController* tab_cluster_ui_controller() const {
    return tab_cluster_ui_controller_.get();
  }
  TabletModeController* tablet_mode_controller() const {
    return tablet_mode_controller_.get();
  }
  TabStripDelegate* tab_strip_delegate() const {
    return tab_strip_delegate_.get();
  }
  ToastManagerImpl* toast_manager() { return toast_manager_.get(); }
  views::corewm::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  ClipboardHistoryControllerImpl* clipboard_history_controller() {
    return clipboard_history_controller_.get();
  }
  TouchDevicesController* touch_devices_controller() {
    return touch_devices_controller_.get();
  }
  AshTouchTransformController* touch_transformer_controller() {
    return touch_transformer_controller_.get();
  }
  TrayAction* tray_action() { return tray_action_.get(); }

  UserMetricsRecorder* metrics() { return user_metrics_recorder_.get(); }

  VideoDetector* video_detector() { return video_detector_.get(); }
  WallpaperControllerImpl* wallpaper_controller() {
    return wallpaper_controller_.get();
  }
  WindowCycleController* window_cycle_controller() {
    return window_cycle_controller_.get();
  }
  WindowRestoreController* window_restore_controller() {
    return window_restore_controller_.get();
  }
  WindowTilingController* window_tiling_controller() {
    return window_tiling_controller_.get();
  }
  OverviewController* overview_controller() {
    return overview_controller_.get();
  }
  WindowTreeHostManager* window_tree_host_manager() {
    return window_tree_host_manager_.get();
  }
  BackGestureEventHandler* back_gesture_event_handler() {
    return back_gesture_event_handler_.get();
  }
  ui::EventHandler* shell_tab_handler() { return shell_tab_handler_.get(); }
  ToplevelWindowEventHandler* toplevel_window_event_handler() {
    return toplevel_window_event_handler_.get();
  }
  AshColorProvider* ash_color_provider() { return ash_color_provider_.get(); }

  PrefService* local_state() { return local_state_; }

  FrameThrottlingController* frame_throttling_controller() {
    return frame_throttling_controller_.get();
  }

  ProjectorControllerImpl* projector_controller() {
    return projector_controller_.get();
  }

  AnnotatorController* annotator_controller() {
    return annotator_controller_.get();
  }

  PciePeripheralNotificationController*
  pcie_peripheral_notification_controller() {
    return pcie_peripheral_notification_controller_.get();
  }

  UsbPeripheralNotificationController*
  usb_peripheral_notification_controller() {
    return usb_peripheral_notification_controller_.get();
  }

  OcclusionTrackerPauser* occlusion_tracker_pauser() {
    return occlusion_tracker_pauser_.get();
  }

  CoralController* coral_controller() { return coral_controller_.get(); }
  CoralDelegate* coral_delegate() { return coral_delegate_.get(); }

  DragDropController* drag_drop_controller() {
    return drag_drop_controller_.get();
  }

  // Does the primary display have status area?
  bool HasPrimaryStatusArea();

  // Starts the animation that occurs on first login.
  void DoInitialWorkspaceAnimation();

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip);

  // Sets a custom color for the cursor.
  void SetCursorColor(SkColor cursor_color);

  // Updates cursor compositing on/off. Native cursor is disabled when cursor
  // compositing is enabled, and vice versa.
  void UpdateCursorCompositingEnabled();

  // Force setting compositing on/off without checking dependency.
  void SetCursorCompositingEnabled(bool enabled);

  // Shows the context menu for the wallpaper or shelf at |location_in_screen|.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  void AddShellObserver(ShellObserver* observer);
  void RemoveShellObserver(ShellObserver* observer);

  // Disables event dispatch during shutdown so that Window events no longer
  // propagate as they are being closed/destroyed.
  void ShutdownEventDispatch();

  // Called when the login status changes.
  // TODO(oshima): Investigate if we can merge this and |OnLoginStateChanged|.
  void UpdateAfterLoginStatusChange(LoginStatus status);

  // Notifies observers that fullscreen mode has changed for |container|.
  // |container| is always the active desk container.
  void NotifyFullscreenStateChanged(bool is_fullscreen,
                                    aura::Window* container);

  // Notifies observers that |pinned_window| changed its pinned window state.
  void NotifyPinnedStateChanged(aura::Window* pinned_window);

  // Notifies observers that |root_window|'s user work area insets have changed.
  // This notification is not fired when shelf bounds changed.
  void NotifyUserWorkAreaInsetsChanged(aura::Window* root_window);

  // Notifies observers that |root_window|'s shelf changed alignment.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfAlignmentChanged(aura::Window* root_window,
                                   ShelfAlignment old_alignment);

  // Notifies observers that the display for new windows has changed.
  void NotifyDisplayForNewWindowsChanged();

  // Adds the |handler| based on its |type| to receive events, ensuring that
  // event handlers continue to be called in their HandlerType order.
  void AddAccessibilityEventHandler(
      ui::EventHandler* handler,
      AccessibilityEventHandlerManager::HandlerType type);

  // Removes |handler| which was added through AddAccessibilityEventHandler.
  void RemoveAccessibilityEventHandler(ui::EventHandler* handler);

  LoginUnlockThroughputRecorder* login_unlock_throughput_recorder() {
    return login_unlock_throughput_recorder_.get();
  }

  // Returns the DeskProfilesDelegate, or nullptr if it isn't available.
  DeskProfilesDelegate* GetDeskProfilesDelegate();

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class AcceleratorControllerTest;
  friend class AshTestHelper;
  friend class RootWindowController;
  friend class ShellTestApi;
  friend class SmsObserverTest;
  friend class ScopedFakeSystemTrayModel;

  explicit Shell(std::unique_ptr<ShellDelegate> shell_delegate);
  ~Shell() override;

  void Init(
      ui::ContextFactory* context_factory,
      PrefService* local_state,
      std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory,
      std::unique_ptr<ash::quick_pair::Mediator::Factory>
          quick_pair_mediator_factory,
      scoped_refptr<dbus::Bus> dbus_bus,
      std::unique_ptr<display::NativeDisplayDelegate> native_display_delegate);

  // Initializes the display manager and related components.
  void InitializeDisplayManager();

  // Initializes the root window so that it can host browser windows.
  void InitRootWindow(aura::Window* root_window);

  // Destroys all child windows including widgets across all roots.
  void CloseAllRootWindowChildWindows();

  // SystemModalContainerEventFilterDelegate:
  bool CanWindowReceiveEvents(aura::Window* window) override;

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // SessionObserver:
  void OnFirstSessionStarted() override;
  void OnFirstSessionReady() override;
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLoginStatusChanged(LoginStatus login_status) override;
  void OnLockStateChanged(bool locked) override;

  static Shell* instance_;

  base::ObserverList<ShellObserver>::Unchecked shell_observers_;

  // The CompoundEventFilter owned by aura::Env object.
  std::unique_ptr<::wm::CompoundEventFilter> env_filter_;

  std::unique_ptr<BatterySaverController> battery_saver_controller_;
  std::unique_ptr<EventRewriterControllerImpl> event_rewriter_controller_;
  std::unique_ptr<InputDeviceSettingsControllerImpl>
      input_device_settings_controller_;
  std::unique_ptr<ModifierKeyComboRecorder> modifier_key_combo_recorder_;
  std::unique_ptr<RapidKeySequenceRecorder> rapid_key_sequence_recorder_;
  std::unique_ptr<InputDeviceSettingsDispatcher>
      input_device_settings_dispatcher_;
  std::unique_ptr<InputDeviceTracker> input_device_tracker_;
  std::unique_ptr<KeyboardModifierMetricsRecorder>
      keyboard_modifier_metrics_recorder_;
  std::unique_ptr<TouchscreenMetricsRecorder> touchscreen_metrics_recorder_;
  std::unique_ptr<InputDeviceKeyAliasManager> input_device_key_alias_manager_;
  std::unique_ptr<ShortcutInputHandler> shortcut_input_handler_;
  std::unique_ptr<UserMetricsRecorder> user_metrics_recorder_;

  std::unique_ptr<AcceleratorPrefs> accelerator_prefs_;
  std::unique_ptr<AshAcceleratorConfiguration> ash_accelerator_configuration_;
  std::unique_ptr<AcceleratorLookup> accelerator_lookup_;
  std::unique_ptr<AcceleratorControllerImpl> accelerator_controller_;
  std::unique_ptr<AcceleratorKeycodeLookupCache>
      accelerator_keycode_lookup_cache_;
  std::unique_ptr<AccessibilityController> accessibility_controller_;
  std::unique_ptr<AccessibilityDelegate> accessibility_delegate_;
  std::unique_ptr<AccessibilityFocusRingControllerImpl>
      accessibility_focus_ring_controller_;
  std::unique_ptr<AdaptiveChargingController> adaptive_charging_controller_;
  std::unique_ptr<AmbientController> ambient_controller_;
  std::unique_ptr<AnchoredNudgeManagerImpl> anchored_nudge_manager_;
  std::unique_ptr<AppListControllerImpl> app_list_controller_;
  std::unique_ptr<AppListFeatureUsageMetrics> app_list_feature_usage_metrics_;
  // May be null in tests or when running on linux-chromeos.
  scoped_refptr<dbus::Bus> dbus_bus_;
  std::unique_ptr<AshDBusServices> ash_dbus_services_;
  std::unique_ptr<AssistantControllerImpl> assistant_controller_;
  std::unique_ptr<AudioEffectsController> audio_effects_controller_;
  std::unique_ptr<AutozoomControllerImpl> autozoom_controller_;
  std::unique_ptr<BacklightsForcedOffSetter> backlights_forced_off_setter_;
  std::unique_ptr<BirchModel> birch_model_;
  std::unique_ptr<BirchPrivacyNudgeController> birch_privacy_nudge_controller_;
  std::unique_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  std::unique_ptr<CalendarController> calendar_controller_;
  std::unique_ptr<CameraEffectsController> camera_effects_controller_;
  std::unique_ptr<ColorPaletteController> color_palette_controller_;
  std::unique_ptr<CrosDisplayConfig> cros_display_config_;
  std::unique_ptr<curtain::SecurityCurtainController>
      security_curtain_controller_;
  std::unique_ptr<DarkLightModeControllerImpl> dark_light_mode_controller_;
  std::unique_ptr<DesksController> desks_controller_;
  std::unique_ptr<SavedDeskController> saved_desk_controller_;
  std::unique_ptr<SavedDeskDelegate> saved_desk_delegate_;
  std::unique_ptr<DetachableBaseHandler> detachable_base_handler_;
  std::unique_ptr<DetachableBaseNotificationController>
      detachable_base_notification_controller_;
  std::unique_ptr<diagnostics::DiagnosticsLogController>
      diagnostics_log_controller_;
  std::unique_ptr<DisplayHighlightController> display_highlight_controller_;
  std::unique_ptr<DisplayPerformanceModeController>
      display_performance_mode_controller_;
  std::unique_ptr<DisplaySpeakerController> display_speaker_controller_;
  std::unique_ptr<DragDropController> drag_drop_controller_;
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;
  std::unique_ptr<FirmwareUpdateNotificationController>
      firmware_update_notification_controller_;
  std::unique_ptr<FocusCycler> focus_cycler_;
  std::unique_ptr<FloatController> float_controller_;
  std::unique_ptr<FocusModeController> focus_mode_controller_;
  std::unique_ptr<GeolocationController> geolocation_controller_;
  std::unique_ptr<BootingAnimationController> booting_animation_controller_;
  std::unique_ptr<GlanceablesController> glanceables_controller_;
  std::unique_ptr<PostLoginGlanceablesMetricsRecorder>
      post_login_glanceables_metrics_reporter_;
  std::unique_ptr<HoldingSpaceController> holding_space_controller_;
  std::unique_ptr<PowerPrefs> power_prefs_;
  std::unique_ptr<SnapGroupController> snap_group_controller_;
  std::unique_ptr<SnoopingProtectionController> snooping_protection_controller_;
  std::unique_ptr<HumanPresenceOrientationController>
      human_presence_orientation_controller_;
  std::unique_ptr<ImeControllerImpl> ime_controller_;
  std::unique_ptr<chromeos::ImmersiveContext> immersive_context_;
  std::unique_ptr<WebAuthNDialogControllerImpl> webauthn_dialog_controller_;
  std::unique_ptr<KeyboardBacklightColorController>
      keyboard_backlight_color_controller_;
  std::unique_ptr<InSessionAuthDialogControllerImpl>
      in_session_auth_dialog_controller_;
  std::unique_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  std::unique_ptr<LobsterController> lobster_controller_;
  std::unique_ptr<LocaleUpdateControllerImpl> locale_update_controller_;
  std::unique_ptr<LoginScreenController> login_screen_controller_;
  std::unique_ptr<LogoutConfirmationController> logout_confirmation_controller_;
  std::unique_ptr<TabClusterUIController> tab_cluster_ui_controller_;
  std::unique_ptr<TabletModeController> tablet_mode_controller_;
  std::unique_ptr<MessageCenterAshImpl> message_center_ash_impl_;
  std::unique_ptr<MediaControllerImpl> media_controller_;
  std::unique_ptr<MediaNotificationProvider> media_notification_provider_;
  std::unique_ptr<MruWindowTracker> mru_window_tracker_;
  std::unique_ptr<MultiDisplayMetricsController>
      multi_display_metrics_controller_;
  std::unique_ptr<MultiDeviceNotificationPresenter>
      multidevice_notification_presenter_;
  std::unique_ptr<chromeos::MultitaskMenuNudgeController::Delegate>
      multitask_menu_nudge_delegate_;
  std::unique_ptr<NearbyShareControllerImpl> nearby_share_controller_;
  std::unique_ptr<NearbyShareDelegate> nearby_share_delegate_;
  std::unique_ptr<ParentAccessController> parent_access_controller_;
  std::unique_ptr<LocalAuthenticationRequestController>
      local_authentication_request_controller_;
  std::unique_ptr<ActiveSessionAuthController> active_session_auth_controller_;
  std::unique_ptr<PciePeripheralNotificationController>
      pcie_peripheral_notification_controller_;
  std::unique_ptr<PickerController> picker_controller_;
  std::unique_ptr<PrivacyHubController> privacy_hub_controller_;
  std::unique_ptr<UsbPeripheralNotificationController>
      usb_peripheral_notification_controller_;
  std::unique_ptr<RgbKeyboardManager> rgb_keyboard_manager_;
  std::unique_ptr<RasterScaleController> raster_scale_controller_;
  std::unique_ptr<ResizeShadowController> resize_shadow_controller_;
  std::unique_ptr<SessionControllerImpl> session_controller_;
  std::unique_ptr<FeatureDiscoveryDurationReporterImpl>
      feature_discover_reporter_;
  std::unique_ptr<AshColorProvider> ash_color_provider_;
  std::unique_ptr<NightLightControllerImpl> night_light_controller_;
  std::unique_ptr<InformedRestoreController> informed_restore_controller_;
  std::unique_ptr<PipController> pip_controller_;
  std::unique_ptr<PrivacyScreenController> privacy_screen_controller_;
  std::unique_ptr<PolicyRecommendationRestorer> policy_recommendation_restorer_;
  std::unique_ptr<ScannerController> scanner_controller_;
  std::unique_ptr<ScreenSwitchCheckController> screen_switch_check_controller_;
  std::unique_ptr<ShelfConfig> shelf_config_;
  std::unique_ptr<ShelfController> shelf_controller_;
  std::unique_ptr<ShelfWindowWatcher> shelf_window_watcher_;
  std::unique_ptr<ShellDelegate> shell_delegate_;
  std::unique_ptr<CaptureModeController> capture_mode_controller_;
  std::unique_ptr<ControlVHistogramRecorder> control_v_histogram_recorder_;
  std::unique_ptr<AcceleratorTracker> accelerator_tracker_;
  std::unique_ptr<ShutdownControllerImpl> shutdown_controller_;
  std::unique_ptr<SystemNotificationController> system_notification_controller_;
  std::unique_ptr<SystemNudgePauseManagerImpl> system_nudge_pause_manager_;
  std::unique_ptr<SystemTrayModel> system_tray_model_;
  std::unique_ptr<SystemTrayNotifier> system_tray_notifier_;
  std::unique_ptr<SystemSoundsDelegate> system_sounds_delegate_;
  std::unique_ptr<api::TasksController> tasks_controller_;
  std::unique_ptr<ToastManagerImpl> toast_manager_;
  std::unique_ptr<ClipboardHistoryControllerImpl> clipboard_history_controller_;
  std::unique_ptr<TouchDevicesController> touch_devices_controller_;
  std::unique_ptr<TrayAction> tray_action_;
  std::unique_ptr<UserEducationController> user_education_controller_;
  std::unique_ptr<TabStripDelegate> tab_strip_delegate_;
  std::unique_ptr<WallpaperControllerImpl> wallpaper_controller_;
  std::unique_ptr<WindowCycleController> window_cycle_controller_;
  std::unique_ptr<WindowRestoreController> window_restore_controller_;
  std::unique_ptr<WindowTilingController> window_tiling_controller_;
  std::unique_ptr<OverviewController> overview_controller_;
  std::unique_ptr<GameDashboardController> game_dashboard_controller_;
  // Owned by |focus_controller_|.
  raw_ptr<AshFocusRules> focus_rules_ = nullptr;
  std::unique_ptr<::wm::ShadowController> shadow_controller_;
  std::unique_ptr<::wm::VisibilityController> visibility_controller_;
  std::unique_ptr<::wm::WindowModalityController> window_modality_controller_;
  raw_ptr<PrefService> local_state_ = nullptr;
  std::unique_ptr<views::corewm::TooltipController> tooltip_controller_;
  std::unique_ptr<PowerButtonController> power_button_controller_;
  std::unique_ptr<LockStateController> lock_state_controller_;
  std::unique_ptr<VideoDetector> video_detector_;
  std::unique_ptr<WindowTreeHostManager> window_tree_host_manager_;
  std::unique_ptr<PersistentWindowController> persistent_window_controller_;
  std::unique_ptr<ColorEnhancementController> color_enhancement_controller_;
  std::unique_ptr<FullscreenMagnifierController>
      fullscreen_magnifier_controller_;
  std::unique_ptr<AutoclickController> autoclick_controller_;
  std::unique_ptr<MouseKeysController> mouse_keys_controller_;
  std::unique_ptr<::wm::FocusController> focus_controller_;

  std::unique_ptr<MouseCursorEventFilter> mouse_cursor_filter_;
  std::unique_ptr<ScreenPositionController> screen_position_controller_;
  std::unique_ptr<SystemModalContainerEventFilter> modality_filter_;
  std::unique_ptr<EventClientImpl> event_client_;
  std::unique_ptr<EventTransformationHandler> event_transformation_handler_;

  // An event filter which handles swiping back from left side of the window.
  std::unique_ptr<BackGestureEventHandler> back_gesture_event_handler_;

  // An event filter which redirects focus when tab is pressed on a RootWindow
  // with no active windows.
  std::unique_ptr<ui::EventHandler> shell_tab_handler_;

  // An event filter which handles moving and resizing windows.
  std::unique_ptr<ToplevelWindowEventHandler> toplevel_window_event_handler_;

  // An event filter which handles system level gestures
  std::unique_ptr<SystemGestureEventFilter> system_gesture_filter_;

  // An event filter that pre-handles global accelerators.
  std::unique_ptr<::wm::AcceleratorFilter> accelerator_filter_;

  std::unique_ptr<display::DisplayManager> display_manager_;
  std::unique_ptr<DisplayPrefs> display_prefs_;
  std::unique_ptr<DisplayConfigurationController>
      display_configuration_controller_;
  std::unique_ptr<DisplayConfigurationObserver> display_configuration_observer_;

  std::unique_ptr<ScreenPinningController> screen_pinning_controller_;

  std::unique_ptr<PeripheralBatteryListener> peripheral_battery_listener_;
  std::unique_ptr<PeripheralBatteryNotifier> peripheral_battery_notifier_;
  std::unique_ptr<PowerEventObserver> power_event_observer_;
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
  std::unique_ptr<VideoActivityNotifier> video_activity_notifier_;
  std::unique_ptr<StickyKeysController> sticky_keys_controller_;
  std::unique_ptr<ResolutionNotificationController>
      resolution_notification_controller_;
  std::unique_ptr<BluetoothNotificationController>
      bluetooth_notification_controller_;
  std::unique_ptr<BluetoothStateCache> bluetooth_state_cache_;
  std::unique_ptr<BluetoothDeviceStatusUiHandler>
      bluetooth_device_status_ui_handler_;
  std::unique_ptr<KeyboardControllerImpl> keyboard_controller_;
  std::unique_ptr<DisplayAlignmentController> display_alignment_controller_;
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  std::unique_ptr<DisplayColorManager> display_color_manager_;
  std::unique_ptr<DisplayErrorObserver> display_error_observer_;
  std::unique_ptr<RefreshRateController> refresh_rate_controller_;
  std::unique_ptr<ProjectingObserver> projecting_observer_;
  std::unique_ptr<HotspotIconAnimation> hotspot_icon_animation_;
  std::unique_ptr<HotspotInfoCache> hotspot_info_cache_;
  std::unique_ptr<display::DisplayPortObserver> display_port_observer_;

  // Listens for output changes and updates the display manager.
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;

  // Listens for shutdown and updates DisplayConfigurator.
  std::unique_ptr<DisplayShutdownObserver> display_shutdown_observer_;

  // Listens for new sms messages and shows notifications.
  std::unique_ptr<SmsObserver> sms_observer_;

  // Implements content::ScreenOrientationController for Chrome OS.
  std::unique_ptr<ScreenOrientationController> screen_orientation_controller_;
  std::unique_ptr<ScreenLayoutObserver> screen_layout_observer_;

  std::unique_ptr<AshTouchTransformController> touch_transformer_controller_;

  std::unique_ptr<ui::EventHandler> magnifier_key_scroll_handler_;
  std::unique_ptr<ui::EventHandler> speech_feedback_handler_;
  std::unique_ptr<LaserPointerController> laser_pointer_controller_;
  std::unique_ptr<PartialMagnifierController> partial_magnifier_controller_;

  std::unique_ptr<DockedMagnifierController> docked_magnifier_controller_;

  std::unique_ptr<chromeos::SnapController> snap_controller_;

  std::unique_ptr<WmModeController> wm_mode_controller_;

  // |native_cursor_manager_| is owned by |cursor_manager_|, but we keep a
  // pointer to vend to test code.
  raw_ptr<NativeCursorManagerAsh, DanglingUntriaged> native_cursor_manager_;

  // Cursor may be hidden on certain key events in Chrome OS, whereas we never
  // hide the cursor on Windows.
  std::unique_ptr<::wm::CursorManager> cursor_manager_;

  // Enables spoken feedback accessibility based on a press and hold of both
  // volume keys.
  std::unique_ptr<KeyAccessibilityEnabler> key_accessibility_enabler_;

  std::unique_ptr<FrameThrottlingController> frame_throttling_controller_;

  std::unique_ptr<ProjectorControllerImpl> projector_controller_;

  std::unique_ptr<AnnotatorController> annotator_controller_;

  std::unique_ptr<AccessibilityEventHandlerManager>
      accessibility_event_handler_manager_;

  // For testing only: simulate that a modal window is open
  bool simulate_modal_window_open_for_test_ = false;

  std::unique_ptr<MessageCenterController> message_center_controller_;

  std::unique_ptr<LoginUnlockThroughputRecorder>
      login_unlock_throughput_recorder_;

  std::unique_ptr<UnlockThroughputRecorder> unlock_throughput_recorder_;

  std::unique_ptr<OcclusionTrackerPauser> occlusion_tracker_pauser_;

  std::unique_ptr<MultiCaptureServiceClient> multi_capture_service_client_;

  std::unique_ptr<federated::FederatedServiceControllerImpl>
      federated_service_controller_;

  std::unique_ptr<quick_pair::Mediator> quick_pair_mediator_;

  std::unique_ptr<display::NativeDisplayDelegate> native_display_delegate_;

  std::unique_ptr<CoralController> coral_controller_;
  std::unique_ptr<CoralDelegate> coral_delegate_;

  base::WeakPtrFactory<Shell> weak_factory_{this};
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::Shell, ash::ShellObserver> {
  static void AddObserver(ash::Shell* source, ash::ShellObserver* observer) {
    source->AddShellObserver(observer);
  }
  static void RemoveObserver(ash::Shell* source, ash::ShellObserver* observer) {
    source->RemoveShellObserver(observer);
  }
};

}  // namespace base

#endif  // ASH_SHELL_H_
