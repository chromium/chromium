// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_observer.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
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

namespace dbus {
class Bus;
}

namespace display {
class DisplayChangeObserver;
class DisplayConfigurator;
class DisplayManager;
}  // namespace display

namespace exo {
class FileHelper;
}  // namespace exo

namespace gfx {
class Insets;
class Point;
}  // namespace gfx

namespace keyboard {
class KeyboardUIFactory;
}

namespace service_manager {
class Connector;
}

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
class UserActivityDetector;
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
class AccessibilityControllerImpl;
class AccessibilityDelegate;
class AccessibilityFocusRingControllerImpl;
class AmbientController;
class AshDBusServices;
class AshFocusRules;
class AppListControllerImpl;
class NativeCursorManagerAsh;
class AshTouchTransformController;
class AssistantController;
class AutoclickController;
class BacklightsForcedOffSetter;
class BluetoothNotificationController;
class BluetoothPowerController;
class BrightnessControlDelegate;
class AshColorProvider;
class CrosDisplayConfig;
class DesksController;
class DetachableBaseHandler;
class DetachableBaseNotificationController;
class DisplayColorManager;
class DisplayConfigurationController;
class DisplayConfigurationObserver;
class DisplayErrorObserver;
class DisplayPrefs;
class DisplayShutdownObserver;
class DisplaySpeakerController;
class DockedMagnifierControllerImpl;
class DragDropController;
class EventClientImpl;
class EventRewriterControllerImpl;
class EventTransformationHandler;
class FocusCycler;
class HighContrastController;
class HighlighterController;
class HomeScreenController;
class ImeController;
class ImmersiveContext;
class KeyAccessibilityEnabler;
class KeyboardBrightnessControlDelegate;
class KeyboardControllerImpl;
class LaserPointerController;
class LocaleUpdateControllerImpl;
class LockStateController;
class LogoutConfirmationController;
class LoginScreenController;
class MagnificationController;
class TabletModeController;
class MediaControllerImpl;
class MediaNotificationControllerImpl;
class MessageCenterController;
class MouseCursorEventFilter;
class MruWindowTracker;
class MultiDeviceNotificationPresenter;
class NightLightControllerImpl;
class OverlayEventFilter;
class OverviewController;
class PartialMagnificationController;
class PeripheralBatteryNotifier;
class PeripheralBatteryTracker;
class PersistentWindowController;
class PolicyRecommendationRestorer;
class PowerButtonController;
class PowerEventObserver;
class PowerPrefs;
class ProjectingObserver;
class ResizeShadowController;
class ResolutionNotificationController;
class RootWindowController;
class ScreenLayoutObserver;
class ScreenOrientationController;
class ScreenshotController;
class ScreenPinningController;
class ScreenPositionController;
class ScreenSwitchCheckController;
class SessionControllerImpl;
class ShelfConfig;
class ShelfController;
class ShelfWindowWatcher;
class ShellDelegate;
struct ShellInitParams;
class ShellObserver;
class ShellState;
class ShutdownControllerImpl;
class SmsObserver;
class SnapController;
class StickyKeysController;
class SystemGestureEventFilter;
class SystemModalContainerEventFilter;
class SystemNotificationController;
class SystemTrayModel;
class SystemTrayNotifier;
class ToastManagerImpl;
class ToplevelWindowEventHandler;
class TouchDevicesController;
class TrayAction;
class TrayBluetoothHelper;
class VideoActivityNotifier;
class VideoDetector;
class WallpaperControllerImpl;
class WaylandServerController;
class WindowCycleController;
class WindowPositioner;
class WindowTreeHostManager;

enum class LoginStatus;

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

  // Returns the root window that newly created windows should be added to.
  // Value can be temporarily overridden using ScopedRootWindowForNewWindows.
  // NOTE: this returns the root, newly created window should be added to the
  // appropriate container in the returned window.
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

  // If necessary, initializes the Wayland server.
  void InitWaylandServer(std::unique_ptr<exo::FileHelper> file_helper);
  void DestroyWaylandServer();

  // Creates a default views::NonClientFrameView for use by windows in the
  // Ash environment.
  views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget);

  // Sets work area insets of the display containing |window|, pings observers.
  void SetDisplayWorkAreaInsets(aura::Window* window,
                                const gfx::Insets& insets);

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // Called when a root window is created.
  void OnRootWindowAdded(aura::Window* root_window);

  // Called when dictation is activated.
  void OnDictationStarted();

  // Called when dictation is ended.
  void OnDictationEnded();

  // Test if TabletModeWindowManager is not enabled, and if
  // TabletModeController is not currently setting a display rotation. Or if
  // the |resolution_notification_controller_| is not showing its confirmation
  // dialog. If true then changes to display settings can be saved.
  bool ShouldSaveDisplaySettings();

  AcceleratorControllerImpl* accelerator_controller() {
    return accelerator_controller_.get();
  }
  AccessibilityControllerImpl* accessibility_controller() {
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
  AmbientController* ambient_controller() { return ambient_controller_.get(); }
  AssistantController* assistant_controller();
  AutoclickController* autoclick_controller() {
    return autoclick_controller_.get();
  }
  BacklightsForcedOffSetter* backlights_forced_off_setter() {
    return backlights_forced_off_setter_.get();
  }
  BluetoothPowerController* bluetooth_power_controller() {
    return bluetooth_power_controller_.get();
  }
  BrightnessControlDelegate* brightness_control_delegate() {
    return brightness_control_delegate_.get();
  }
  service_manager::Connector* connector() { return connector_; }
  CrosDisplayConfig* cros_display_config() {
    return cros_display_config_.get();
  }
  ::wm::CursorManager* cursor_manager() { return cursor_manager_.get(); }
  DesksController* desks_controller() { return desks_controller_.get(); }
  DetachableBaseHandler* detachable_base_handler() {
    return detachable_base_handler_.get();
  }

  display::DisplayManager* display_manager() { return display_manager_.get(); }
  DisplayPrefs* display_prefs() { return display_prefs_.get(); }
  DisplayConfigurationController* display_configuration_controller() {
    return display_configuration_controller_.get();
  }

  display::DisplayConfigurator* display_configurator();

  DisplayColorManager* display_color_manager() {
    return display_color_manager_.get();
  }
  DisplayErrorObserver* display_error_observer() {
    return display_error_observer_.get();
  }

  DockedMagnifierControllerImpl* docked_magnifier_controller();
  ::wm::CompoundEventFilter* env_filter() { return env_filter_.get(); }
  EventRewriterControllerImpl* event_rewriter_controller() {
    return event_rewriter_controller_.get();
  }
  EventClientImpl* event_client() { return event_client_.get(); }
  EventTransformationHandler* event_transformation_handler() {
    return event_transformation_handler_.get();
  }
  HomeScreenController* home_screen_controller() {
    return home_screen_controller_.get();
  }
  ::wm::FocusController* focus_controller() { return focus_controller_.get(); }
  AshFocusRules* focus_rules() { return focus_rules_; }
  FocusCycler* focus_cycler() { return focus_cycler_.get(); }
  HighlighterController* highlighter_controller() {
    return highlighter_controller_.get();
  }
  HighContrastController* high_contrast_controller() {
    return high_contrast_controller_.get();
  }
  ImeController* ime_controller() { return ime_controller_.get(); }
  KeyAccessibilityEnabler* key_accessibility_enabler() {
    return key_accessibility_enabler_.get();
  }
  KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate() {
    return keyboard_brightness_control_delegate_.get();
  }
  KeyboardControllerImpl* keyboard_controller() {
    return keyboard_controller_.get();
  }
  LaserPointerController* laser_pointer_controller() {
    return laser_pointer_controller_.get();
  }
  LocaleUpdateControllerImpl* locale_update_controller() {
    return locale_update_controller_.get();
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
  MagnificationController* magnification_controller() {
    return magnification_controller_.get();
  }
  MediaControllerImpl* media_controller() { return media_controller_.get(); }
  MediaNotificationControllerImpl* media_notification_controller() {
    return media_notification_controller_.get();
  }
  MessageCenterController* message_center_controller() {
    return message_center_controller_.get();
  }
  MouseCursorEventFilter* mouse_cursor_filter() {
    return mouse_cursor_filter_.get();
  }
  MruWindowTracker* mru_window_tracker() { return mru_window_tracker_.get(); }
  NightLightControllerImpl* night_light_controller() {
    return night_light_controller_.get();
  }
  OverlayEventFilter* overlay_filter() { return overlay_filter_.get(); }
  PartialMagnificationController* partial_magnification_controller() {
    return partial_magnification_controller_.get();
  }
  PolicyRecommendationRestorer* policy_recommendation_restorer() {
    return policy_recommendation_restorer_.get();
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }
  PowerEventObserver* power_event_observer() {
    return power_event_observer_.get();
  }
  ResizeShadowController* resize_shadow_controller() {
    return resize_shadow_controller_.get();
  }
  ResolutionNotificationController* resolution_notification_controller() {
    return resolution_notification_controller_.get();
  }
  ScreenshotController* screenshot_controller() {
    return screenshot_controller_.get();
  }
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
  ::wm::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }
  ShelfConfig* shelf_config() { return shelf_config_.get(); }
  ShelfController* shelf_controller() { return shelf_controller_.get(); }
  ShellDelegate* shell_delegate() { return shell_delegate_.get(); }
  ShellState* shell_state() { return shell_state_.get(); }
  ShutdownControllerImpl* shutdown_controller() {
    return shutdown_controller_.get();
  }
  StickyKeysController* sticky_keys_controller() {
    return sticky_keys_controller_.get();
  }
  SystemNotificationController* system_notification_controller() {
    return system_notification_controller_.get();
  }
  SystemTrayModel* system_tray_model() { return system_tray_model_.get(); }
  SystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }
  TabletModeController* tablet_mode_controller() {
    return tablet_mode_controller_.get();
  }
  ToastManagerImpl* toast_manager() { return toast_manager_.get(); }
  views::corewm::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  TouchDevicesController* touch_devices_controller() {
    return touch_devices_controller_.get();
  }
  AshTouchTransformController* touch_transformer_controller() {
    return touch_transformer_controller_.get();
  }
  TrayAction* tray_action() { return tray_action_.get(); }
  TrayBluetoothHelper* tray_bluetooth_helper() {
    return tray_bluetooth_helper_.get();
  }
  UserMetricsRecorder* metrics() { return user_metrics_recorder_.get(); }
  VideoDetector* video_detector() { return video_detector_.get(); }
  WallpaperControllerImpl* wallpaper_controller() {
    return wallpaper_controller_.get();
  }
  WindowCycleController* window_cycle_controller() {
    return window_cycle_controller_.get();
  }
  WindowPositioner* window_positioner() { return window_positioner_.get(); }
  OverviewController* overview_controller() {
    return overview_controller_.get();
  }
  WindowTreeHostManager* window_tree_host_manager() {
    return window_tree_host_manager_.get();
  }
  ToplevelWindowEventHandler* toplevel_window_event_handler() {
    return toplevel_window_event_handler_.get();
  }
  AshColorProvider* ash_color_provider() { return ash_color_provider_.get(); }

  PrefService* local_state() { return local_state_; }

  // Force the shelf to query for it's current visibility state.
  // TODO(jamescook): Move to Shelf.
  void UpdateShelfVisibility();

  // Does the primary display have status area?
  bool HasPrimaryStatusArea();

  // Starts the animation that occurs on first login.
  void DoInitialWorkspaceAnimation();

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip);

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
  void NotifyShelfAlignmentChanged(aura::Window* root_window);

  // Notifies observers that |root_window|'s shelf changed auto-hide behavior.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfAutoHideBehaviorChanged(aura::Window* root_window);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class AcceleratorControllerTest;
  friend class AshTestHelper;
  friend class RootWindowController;
  friend class ScopedRootWindowForNewWindows;
  friend class ShellTestApi;
  friend class SmsObserverTest;

  Shell(std::unique_ptr<ShellDelegate> shell_delegate,
        service_manager::Connector* connector);
  ~Shell() override;

  void Init(ui::ContextFactory* context_factory,
            ui::ContextFactoryPrivate* context_factory_private,
            PrefService* local_state,
            std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory,
            scoped_refptr<dbus::Bus> dbus_bus);

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
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLoginStatusChanged(LoginStatus login_status) override;
  void OnLockStateChanged(bool locked) override;

  static Shell* instance_;

  // The CompoundEventFilter owned by aura::Env object.
  std::unique_ptr<::wm::CompoundEventFilter> env_filter_;

  std::unique_ptr<EventRewriterControllerImpl> event_rewriter_controller_;

  std::unique_ptr<UserMetricsRecorder> user_metrics_recorder_;
  std::unique_ptr<WindowPositioner> window_positioner_;

  std::unique_ptr<AcceleratorControllerImpl> accelerator_controller_;
  std::unique_ptr<AccessibilityControllerImpl> accessibility_controller_;
  std::unique_ptr<AccessibilityDelegate> accessibility_delegate_;
  std::unique_ptr<AccessibilityFocusRingControllerImpl>
      accessibility_focus_ring_controller_;
  std::unique_ptr<AmbientController> ambient_controller_;
  std::unique_ptr<AppListControllerImpl> app_list_controller_;
  // May be null in tests or when running on linux-chromeos.
  scoped_refptr<dbus::Bus> dbus_bus_;
  std::unique_ptr<AshDBusServices> ash_dbus_services_;
  std::unique_ptr<AssistantController> assistant_controller_;
  std::unique_ptr<BacklightsForcedOffSetter> backlights_forced_off_setter_;
  std::unique_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  std::unique_ptr<CrosDisplayConfig> cros_display_config_;
  service_manager::Connector* const connector_;
  std::unique_ptr<DesksController> desks_controller_;
  std::unique_ptr<DetachableBaseHandler> detachable_base_handler_;
  std::unique_ptr<DetachableBaseNotificationController>
      detachable_base_notification_controller_;
  std::unique_ptr<DisplaySpeakerController> display_speaker_controller_;
  std::unique_ptr<DragDropController> drag_drop_controller_;
  std::unique_ptr<FocusCycler> focus_cycler_;
  std::unique_ptr<HomeScreenController> home_screen_controller_;
  std::unique_ptr<ImeController> ime_controller_;
  std::unique_ptr<ImmersiveContext> immersive_context_;
  std::unique_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  std::unique_ptr<LocaleUpdateControllerImpl> locale_update_controller_;
  std::unique_ptr<LoginScreenController> login_screen_controller_;
  std::unique_ptr<LogoutConfirmationController> logout_confirmation_controller_;
  std::unique_ptr<TabletModeController> tablet_mode_controller_;
  std::unique_ptr<MediaControllerImpl> media_controller_;
  std::unique_ptr<MediaNotificationControllerImpl>
      media_notification_controller_;
  std::unique_ptr<MruWindowTracker> mru_window_tracker_;
  std::unique_ptr<MultiDeviceNotificationPresenter>
      multidevice_notification_presenter_;
  std::unique_ptr<ResizeShadowController> resize_shadow_controller_;
  std::unique_ptr<AshColorProvider> ash_color_provider_;
  std::unique_ptr<SessionControllerImpl> session_controller_;
  std::unique_ptr<NightLightControllerImpl> night_light_controller_;
  std::unique_ptr<PolicyRecommendationRestorer> policy_recommendation_restorer_;
  std::unique_ptr<ScreenSwitchCheckController> screen_switch_check_controller_;
  std::unique_ptr<ShelfConfig> shelf_config_;
  std::unique_ptr<ShelfController> shelf_controller_;
  std::unique_ptr<ShelfWindowWatcher> shelf_window_watcher_;
  std::unique_ptr<ShellDelegate> shell_delegate_;
  std::unique_ptr<ShellState> shell_state_;
  std::unique_ptr<ShutdownControllerImpl> shutdown_controller_;
  std::unique_ptr<SystemNotificationController> system_notification_controller_;
  std::unique_ptr<SystemTrayModel> system_tray_model_;
  std::unique_ptr<SystemTrayNotifier> system_tray_notifier_;
  std::unique_ptr<ToastManagerImpl> toast_manager_;
  std::unique_ptr<TouchDevicesController> touch_devices_controller_;
  std::unique_ptr<TrayAction> tray_action_;
  std::unique_ptr<WallpaperControllerImpl> wallpaper_controller_;
  std::unique_ptr<WindowCycleController> window_cycle_controller_;
  std::unique_ptr<OverviewController> overview_controller_;
  // Owned by |focus_controller_|.
  AshFocusRules* focus_rules_ = nullptr;
  std::unique_ptr<::wm::ShadowController> shadow_controller_;
  std::unique_ptr<::wm::VisibilityController> visibility_controller_;
  std::unique_ptr<::wm::WindowModalityController> window_modality_controller_;
  PrefService* local_state_ = nullptr;
  std::unique_ptr<views::corewm::TooltipController> tooltip_controller_;
  std::unique_ptr<PowerButtonController> power_button_controller_;
  std::unique_ptr<LockStateController> lock_state_controller_;
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
  std::unique_ptr<VideoDetector> video_detector_;
  std::unique_ptr<WaylandServerController> wayland_server_controller_;
  std::unique_ptr<WindowTreeHostManager> window_tree_host_manager_;
  std::unique_ptr<PersistentWindowController> persistent_window_controller_;
  std::unique_ptr<HighContrastController> high_contrast_controller_;
  std::unique_ptr<MagnificationController> magnification_controller_;
  std::unique_ptr<AutoclickController> autoclick_controller_;
  std::unique_ptr<::wm::FocusController> focus_controller_;

  std::unique_ptr<ScreenshotController> screenshot_controller_;

  std::unique_ptr<MouseCursorEventFilter> mouse_cursor_filter_;
  std::unique_ptr<ScreenPositionController> screen_position_controller_;
  std::unique_ptr<SystemModalContainerEventFilter> modality_filter_;
  std::unique_ptr<EventClientImpl> event_client_;
  std::unique_ptr<EventTransformationHandler> event_transformation_handler_;

  // An event filter that pre-handles key events while the partial
  // screenshot UI or the keyboard overlay is active.
  std::unique_ptr<OverlayEventFilter> overlay_filter_;

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

  std::unique_ptr<PeripheralBatteryNotifier> peripheral_battery_notifier_;
  std::unique_ptr<PeripheralBatteryTracker> peripheral_battery_tracker_;
  std::unique_ptr<PowerEventObserver> power_event_observer_;
  std::unique_ptr<PowerPrefs> power_prefs_;
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
  std::unique_ptr<VideoActivityNotifier> video_activity_notifier_;
  std::unique_ptr<StickyKeysController> sticky_keys_controller_;
  std::unique_ptr<ResolutionNotificationController>
      resolution_notification_controller_;
  std::unique_ptr<BluetoothNotificationController>
      bluetooth_notification_controller_;
  std::unique_ptr<BluetoothPowerController> bluetooth_power_controller_;
  std::unique_ptr<TrayBluetoothHelper> tray_bluetooth_helper_;
  std::unique_ptr<KeyboardControllerImpl> keyboard_controller_;
  std::unique_ptr<DisplayColorManager> display_color_manager_;
  std::unique_ptr<DisplayErrorObserver> display_error_observer_;
  std::unique_ptr<ProjectingObserver> projecting_observer_;

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
  std::unique_ptr<PartialMagnificationController>
      partial_magnification_controller_;
  std::unique_ptr<HighlighterController> highlighter_controller_;

  std::unique_ptr<DockedMagnifierControllerImpl> docked_magnifier_controller_;

  std::unique_ptr<SnapController> snap_controller_;

  // |native_cursor_manager_| is owned by |cursor_manager_|, but we keep a
  // pointer to vend to test code.
  NativeCursorManagerAsh* native_cursor_manager_;

  // Cursor may be hidden on certain key events in Chrome OS, whereas we never
  // hide the cursor on Windows.
  std::unique_ptr<::wm::CursorManager> cursor_manager_;

  // Enables spoken feedback accessibility based on a press and hold of both
  // volume keys.
  std::unique_ptr<KeyAccessibilityEnabler> key_accessibility_enabler_;

  // For testing only: simulate that a modal window is open
  bool simulate_modal_window_open_for_test_ = false;

  std::unique_ptr<MessageCenterController> message_center_controller_;

  base::ObserverList<ShellObserver>::Unchecked shell_observers_;

  base::WeakPtrFactory<Shell> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
