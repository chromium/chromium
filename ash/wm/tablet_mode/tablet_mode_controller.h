// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_

#include <memory>
#include <optional>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/ash_export.h"
#include "ash/bluetooth_devices_observer.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell_observer.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/screen.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace aura {
class Window;
}

namespace base {
class TickClock;
}

namespace gfx {
class Vector3dF;
}

namespace ui {
class LayerAnimationSequence;
}

namespace views {
class Widget;
}

namespace ash {

class InternalInputDevicesEventBlocker;
class TabletModeObserver;
class TabletModeWindowManager;

// TODO(b/357489575): cleanup this kill-switch.
BASE_DECLARE_FEATURE(kBlockUiTabletModeInKiosk);

// When EC (Embedded Controller) cannot handle lid angle calculation,
// TabletModeController listens to accelerometer events and automatically
// enters and exits tablet mode when the lid is opened beyond the triggering
// angle and rotates the display to match the device when in tablet mode.
class ASH_EXPORT TabletModeController
    : public AccelerometerReader::Observer,
      public chromeos::PowerManagerClient::Observer,
      public TabletMode,
      public ShellObserver,
      public display::DisplayManagerObserver,
      public SessionObserver,
      public ui::InputDeviceEventObserver,
      public ui::LayerAnimationObserver,
      public ui::LayerObserver {
 public:
  // Used for keeping track if the user wants the machine to behave as a
  // clamshell/tablet regardless of hardware orientation.
  // TODO(oshima): Move this to common place.
  enum class UiMode {
    kNone = 0,
    kClamshell,
    kTabletMode,
  };

  // Public so it can be used by unit tests.
  constexpr static char kLidAngleHistogramName[] = "Ash.TouchView.LidAngle";
  constexpr static char kTabletInactiveTimeHistogramName[] =
      "Ash.TouchView.TouchViewInactive";
  constexpr static char kTabletActiveTimeHistogramName[] =
      "Ash.TouchView.TouchViewActive";

  // Enable or disable using a screenshot for testing as it makes the
  // initialization flow async, which makes most tests harder to write.
  static void SetUseScreenshotForTest(bool use_screenshot);

  // Returns the the animation property that we observe when transitioning from
  // clamshell to tablet mode.
  static ui::LayerAnimationElement::AnimatableProperty
  GetObservedTabletTransitionProperty();

  TabletModeController();

  TabletModeController(const TabletModeController&) = delete;
  TabletModeController& operator=(const TabletModeController&) = delete;

  ~TabletModeController() override;

  void Shutdown();

  // Add a special window to the TabletModeWindowManager for tracking. This is
  // only required for special windows which are handled by other window
  // managers like the |MultiUserWindowManagerImpl|.
  // If the tablet mode is not enabled no action will be performed.
  void AddWindow(aura::Window* window);

  // Checks if we should auto hide title bars for the |widget| in tablet mode.
  bool ShouldAutoHideTitlebars(views::Widget* widget);

  // If |record_lid_angle_timer_| is running, invokes its task and returns true.
  // Otherwise, returns false.
  [[nodiscard]] bool TriggerRecordLidAngleTimerForTesting();

  // Starts observing |window| for animation changes.
  void MaybeObserveBoundsAnimation(aura::Window* window);

  // Stops observing the window which is being animated from tablet <->
  // clamshell.
  void StopObservingAnimation(bool record_stats, bool delete_screenshot);

  // Returns true if we're in tablet mode for development purpose (please refer
  // to kOnForDev for more details.)
  bool IsInDevTabletMode() const;

  // TabletMode:
  void AddObserver(TabletModeObserver* observer) override;
  void RemoveObserver(TabletModeObserver* observer) override;
  bool AreInternalInputDeviceEventsBlocked() const override;
  bool ForceUiTabletModeState(std::optional<bool> enabled) override;
  // Do NOT call this directly from unit tests. Instead, please use
  // ash::TabletModeControllerTestApi().{Enter/Leave}TabletMode().
  // TODO(crbug.com/40942452): Move this to private.
  void SetEnabledForTest(bool enabled) override;

  // ShellObserver:
  void OnShellInitialized() override;

  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus login_status) override;
  void OnChromeTerminating() override;

  // AccelerometerReader::Observer:
  void OnECLidAngleDriverStatusChanged(bool is_supported) override;
  void OnAccelerometerUpdated(const AccelerometerUpdate& update) override;

  // chromeos::PowerManagerClient::Observer:
  void PowerManagerBecameAvailable(bool available) override;
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        base::TimeTicks time) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               base::TimeTicks time) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;
  void OnDeviceListsComplete() override;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  void increment_app_window_drag_count() { ++app_window_drag_count_; }
  void increment_app_window_drag_in_splitview_count() {
    ++app_window_drag_in_splitview_count_;
  }
  void increment_tab_drag_count() { ++tab_drag_count_; }
  void increment_tab_drag_in_splitview_count() {
    ++tab_drag_in_splitview_count_;
  }

  TabletModeWindowManager* tablet_mode_window_manager() {
    return tablet_mode_window_manager_.get();
  }

  bool is_in_tablet_physical_state() const {
    return is_in_tablet_physical_state_;
  }

  float lid_angle() const { return lid_angle_; }

  // Enable/disable the tablet mode for development. Please see cc file
  // for more details.
  void SetEnabledForDev(bool enabled);

  // Returns true if the system tray should have a overview button.
  bool ShouldShowOverviewButton() const;

  // True if it is possible to enter tablet mode in the current
  // configuration. If this returns false, it should never be the case that
  // tablet mode becomes enabled.
  bool CanEnterTabletMode() const;

  // ForcePhysicalTabletState is to control physical tablet state. The default
  // state is not to force the state, so the tablet-mode controller will observe
  // device configurations.
  enum class ForcePhysicalTabletState {
    kDefault,
    kForceTabletMode,
    kForceClamshellMode,
  };

  // Defines how the tablet mode controller controls the
  // tablet mode and its transition between clamshell mode.
  // This is defined as a public to define constexpr in cc.
  struct TabletModeBehavior {
    bool use_sensor = true;
    bool observe_display_events = true;
    bool observe_pointer_device_events = true;
    bool block_internal_input_device = false;
    bool always_show_overview_button = false;
    ForcePhysicalTabletState force_physical_tablet_state =
        ForcePhysicalTabletState::kDefault;

    bool operator==(const TabletModeBehavior& other) const {
      return use_sensor == other.use_sensor &&
             observe_display_events == other.observe_display_events &&
             observe_pointer_device_events ==
                 other.observe_pointer_device_events &&
             block_internal_input_device == other.block_internal_input_device &&
             always_show_overview_button == other.always_show_overview_button &&
             force_physical_tablet_state == other.force_physical_tablet_state;
    }
  };

 private:
  class DestroyObserver;
  class ScopedContainerHider;
  friend class TabletModeControllerTestApi;

  // Used for recording metrics for intervals of time spent in
  // and out of TabletMode.
  enum TabletModeIntervalType {
    TABLET_MODE_INTERVAL_INACTIVE,
    TABLET_MODE_INTERVAL_ACTIVE
  };

  // Turn the always tablet mode window manager on or off.
  void SetTabletModeEnabledInternal(bool should_enable);

  // If EC cannot handle lid angle calc, browser detects hinge rotation from
  // base and lid accelerometers and automatically start / stop tablet mode.
  void HandleHingeRotation(const AccelerometerUpdate& update);

  void OnGetSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> result);

  // Returns true if unstable lid angle can be used. The lid angle that falls in
  // the unstable zone ([0, 20) and (340, 360] degrees) is considered unstable
  // due to the potential erroneous accelerometer readings. Immediately using
  // the unstable angle to trigger tablet mode is error-prone. So we wait for
  // a certain range of time before using unstable angle.
  bool CanUseUnstableLidAngle() const;

  // Record UMA stats tracking TabletMode usage. If |type| is
  // TABLET_MODE_INTERVAL_INACTIVE, then record that TabletMode has been
  // inactive from |tablet_mode_usage_interval_start_time_| until now.
  // Similarly, record that TabletMode has been active if |type| is
  // TABLET_MODE_INTERVAL_ACTIVE.
  void RecordTabletModeUsageInterval(TabletModeIntervalType type);

  // Reports an UMA histogram containing the value of |lid_angle_|.
  // Called periodically by |record_lid_angle_timer_|. If EC can handle lid
  // angle calc, |lid_angle_| is unavailable to browser.
  void RecordLidAngle();

  // Returns TABLET_MODE_INTERVAL_ACTIVE if TabletMode is currently active,
  // otherwise returns TABLET_MODE_INTERNAL_INACTIVE.
  TabletModeIntervalType CurrentTabletModeIntervalType();

  // Called when a pointing device config is changed, or when a device list is
  // sent from device manager. This will exit tablet mode if needed.
  void HandlePointingDeviceAddedOrRemoved();

  // Callback function of |bluetooth_devices_observer_|. Called when the
  // bluetooth adapter or |device| changes.
  void OnBluetoothAdapterOrDeviceChanged(device::BluetoothDevice* device);

  // Update the internal mouse and keyboard event blocker |event_blocker_|
  // according to current configuration. The internal input events should be
  // blocked if 1) we are currently in tablet mode or 2) we are currently in
  // laptop mode but the lid is flipped over (i.e., we are in laptop mode
  // because of an external attached mouse).
  void UpdateInternalInputDevicesEventBlocker();

  // Suspends |occlusion_tracker_pauser_| for the duration of
  // kOcclusionTrackTimeout.
  void SuspendOcclusionTracker();

  // Resets |occlusion_tracker_pauser_|.
  void ResetPauser();

  // Deletes the enter tablet mode screenshot and associated callbacks.
  void DeleteScreenshot();

  void ResetDestroyObserver();

  // Finishes initializing for tablet mode. May be called async if a screenshot
  // was requested while starting initializing.
  void FinishInitTabletMode();

  // Takes a screenshot of everything in the rotation container, except for
  // |top_window|.
  void TakeScreenshot(aura::Window* top_window);

  // Called when a screenshot is taken. Creates |screenshot_widget_| which holds
  // the screenshot results and stacks it under top window. |root_window|
  // specifies on which root window the screen shot is taken.
  void OnLayerCopyed(base::OnceClosure on_screenshot_taken,
                     aura::Window* root_window,
                     std::unique_ptr<ui::Layer> copy_layer);

  // Calculates whether the device is currently in a physical tablet state,
  // using the most recent seen device events such as lid angle changes.
  bool CalculateIsInTabletPhysicalState() const;

  // Returns whether the UI should be in tablet mode based on the current
  // physical tablet state, the availability of external input devices, and
  // whether the UI is forced in a particular mode via command-line flags.
  bool ShouldUiBeInTabletMode() const;

  // Sets |is_in_tablet_physical_state_| to |new_state| and potentially updating
  // the UI tablet mode state if needed. Returns true if the
  // |is_in_tablet_physical_state_| has been changed.
  bool SetIsInTabletPhysicalState(bool new_state);

  // Updates the UI by either entering or exiting UI tablet mode if necessary
  // based on the current state. Returns true if there's a change in the UI
  // tablet mode state, false otherwise.
  bool UpdateUiTabletState();

  // Starts tracking the tablet usage metrics if the following conditions are
  // all meet:
  // 1. The device is capable of entering tablet mode.
  // 2. The device has seen accelerometer data or the device has EC lid angle
  //    driver supported.
  // 3. The device has seen tablet mode event and has responded to tablet mode
  //    event.
  // 4. Initial input device setup has been finished. At this moment, we know
  //    the device has responded to the input device change.
  // 5. We haven't started tracking the tablet usage metrics.
  // The conditions 1, 2, 3, 4 are to avoid the false recordings that can happen
  // at startup. During startup, since all these above events are async, plus
  // potential sensor noises, the device can change its ui mode a couple times
  // before it stabilized to its correct ui mode, thus we don't want to log the
  // tablet usage metrics before the device has received all necessary events
  // and has stabilized its ui mode.
  void StartTrackingTabletUsageMetricsIfApplicable();

  // The tablet window manager (if enabled).
  std::unique_ptr<TabletModeWindowManager> tablet_mode_window_manager_;

  // A helper class which when instantiated will block native events from the
  // internal keyboard and touchpad.
  std::unique_ptr<InternalInputDevicesEventBlocker> event_blocker_;

  // Whether we have ever seen accelerometer data. When ChromeOS EC lid angle
  // driver is supported, convertible device cannot see accelerometer data.
  bool have_seen_accelerometer_data_ = false;

  // Whether we have ever seen tablet mode event sent from power manager.
  bool have_seen_tablet_mode_event_ = false;

  // If ECLidAngleDriverStatus is supported, Chrome does not calculate lid angle
  // itself, but will rely on the tablet-mode flag that EC sends to decide if
  // the device should in tablet mode.
  // As it's set in |OnECLidAngleDriverStatusChanged|, which is a callback by
  // AccelerometerReader, we make it optional to indicate a lack of value until
  // the accelerometer reader is initialized.
  std::optional<bool> is_ec_lid_angle_driver_supported_;

  // Whether the lid angle can be detected by browser. If it's true, the device
  // is a convertible device (both screen acclerometer and keyboard acclerometer
  // are available), and doesn't have ChromeOS EC lid angle driver, in this way
  // lid angle should be calculated by browser. And if it's false, the device is
  // probably a convertible device with ChromeOS EC lid angle driver, or the
  // device is either a laptop device or a tablet device (only the screen
  // acclerometer is available).
  bool can_detect_lid_angle_ = false;

  // Tracks time spent in (and out of) tablet mode.
  base::Time tablet_mode_usage_interval_start_time_;
  base::TimeDelta total_tablet_mode_time_;
  base::TimeDelta total_non_tablet_mode_time_;

  // Tracks the first time the lid angle was unstable. This is used to suppress
  // erroneous accelerometer readings as the lid is nearly opened or closed but
  // the accelerometer reports readings that make the lid to appear near fully
  // open. (e.g. After closing the lid, the correct angle reading is 0. But the
  // accelerometer may report 359.5 degrees which triggers the tablet mode by
  // mistake.)
  base::TimeTicks first_unstable_lid_angle_time_;

  // Source for the current time in base::TimeTicks.
  raw_ptr<const base::TickClock> tick_clock_;

  // Forces the UI mode to be in tablet or clamsell state. Can be forced via:
  //   1) command-line flags, such as `--force-tablet-mode=touch_view` or
  //   `--force-tablet-mode=clamshell`.
  //   2) observing `OnLoginStatusChanged`, since Ui tablet mode is blocked in
  //   Kiosk.
  UiMode forced_ui_mode_ = UiMode::kNone;

  // True if the device is physically in a tablet state regardless of the UI
  // tablet mode state. The physical tablet state only changes based on device
  // events such as lid angle changes, or device getting detached from its base.
  bool is_in_tablet_physical_state_ = false;

  // Set when tablet mode switch is on. This is used to force tablet mode.
  bool tablet_mode_switch_is_on_ = false;

  // Tracks when the lid is closed. Used to prevent entering tablet mode.
  bool lid_is_closed_ = false;

  // True if |lid_angle_| is in the stable range of angle values.
  // (See kMinStableAngle and kMaxStableAngle).
  bool lid_angle_is_stable_ = false;

  // Last computed lid angle.
  double lid_angle_ = 0.0f;

  // Tracks if the device has an external pointing device. The device will
  // not enter tablet mode if this is true.
  bool has_external_pointing_device_ = false;

  // Tracks if the device has an internal pointing device. The device will not
  // enter clamshell mode if both |has_internal_pointing_device_| and
  // |has_external_pointing_device_| are false only for tablet-capable devices.
  bool has_internal_pointing_device_ = true;

  // Set to true temporarily when the tablet mode is enabled/disabled via the
  // developer's keyboard shortcut in order to update the visibility of the
  // overview tray button, even though internal events are not blocked.
  bool force_notify_events_blocking_changed_ = false;

  // Counts the app window drag from top in tablet mode.
  int app_window_drag_count_ = 0;

  // Counts the app window drag from top when splitview is active.
  int app_window_drag_in_splitview_count_ = 0;

  // Counts of the tab drag from top in tablet mode, includes both non-source
  // window and source window drag.
  int tab_drag_count_ = 0;

  // Counts of the tab drag from top when splitview is active.
  int tab_drag_in_splitview_count_ = 0;

  // Tracks smoothed accelerometer data over time. This is done when the hinge
  // is approaching vertical to remove abrupt acceleration that can lead to
  // incorrect calculations of hinge angles.
  gfx::Vector3dF base_smoothed_;
  gfx::Vector3dF lid_smoothed_;

  // Calls RecordLidAngle() periodically.
  base::RepeatingTimer record_lid_angle_timer_;

  ScopedSessionObserver scoped_session_observer_{this};

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;

  // Starts when enter/exit tablet mode. Runs ResetPauser to reset the
  // occlustion tracker.
  base::OneShotTimer occlusion_tracker_reset_timer_;

  // Observer to observe the bluetooth devices.
  std::unique_ptr<BluetoothDevicesObserver> bluetooth_devices_observer_;

  // Observers top windows or animating window during state transition.
  std::unique_ptr<DestroyObserver> destroy_observer_;

  // The layer that animates duraing tablet mode <-> clamshell
  // transition. It's observed to take an action after its animation ends.
  raw_ptr<ui::Layer> animating_layer_ = nullptr;

  // When in scope, hides the shelf and float containers. Used to temporarily
  // hide shelf while taking a screenshot during tablet mode transition (so the
  // screenshot does not show the old version of the shelf and floated window in
  // the background).
  std::unique_ptr<ScopedContainerHider> container_hider_;

  // Tracks and record transition smoothness.
  std::optional<ui::ThroughputTracker> transition_tracker_;

  base::CancelableOnceCallback<void(std::unique_ptr<ui::Layer>)>
      screenshot_taken_callback_;
  base::CancelableOnceClosure screenshot_set_callback_;

  // A layer that is created before an enter tablet mode animations is started,
  // and destroyed when the animation is ended. It contains a screenshot of
  // everything in the screen rotation container except the top window. It helps
  // with animation performance because it fully occludes all windows except the
  // animating window for the duration of the animation.
  // TODO(sammiequon): See if we can move screenshot and tablet mode transition
  // animation related code into a separate class/file.
  std::unique_ptr<ui::Layer> screenshot_layer_;

  base::ObserverList<TabletModeObserver>::Unchecked tablet_mode_observers_;

  TabletModeBehavior tablet_mode_behavior_;

  // True if the initial input device setup has been finished. Only after it's
  // finished, we'll start monitoring input device add/remove events and respond
  // to these events to enter/exit tablet mode accordingly.
  bool initial_input_device_set_up_finished_ = false;

  base::WeakPtrFactory<TabletModeController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_
