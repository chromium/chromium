// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_menu/menu_util.h"
#include "ash/constants/ash_switches.h"
#include "ash/login_status.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/utility/layer_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/internal_input_devices_event_blocker.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_util.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

BASE_FEATURE(kBlockUiTabletModeInKiosk,
             "BlockUiTabletModeInKiosk",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// The hinge angle at which to enter tablet mode.
constexpr float kEnterTabletModeAngle = 200.0f;

// The angle at which to exit tablet mode, this is specifically less than the
// angle to enter tablet mode to prevent rapid toggling when near the angle.
constexpr float kExitTabletModeAngle = 160.0f;

// Defines a range for which accelerometer readings are considered accurate.
// When the lid is near open (or near closed) the accelerometer readings may be
// inaccurate and a lid that is fully open may appear to be near closed (and
// vice versa).
constexpr float kMinStableAngle = 20.0f;
constexpr float kMaxStableAngle = 340.0f;

// The time duration to consider an unstable lid angle to be valid. This is used
// to prevent entering tablet mode if an erroneous accelerometer reading makes
// the lid appear to be fully open when the user is opening the lid from a
// closed position or is closing the lid from an opened position.
constexpr base::TimeDelta kUnstableLidAngleDuration = base::Seconds(2);

// When the device approaches vertical orientation (i.e. portrait orientation)
// the accelerometers for the base and lid approach the same values (i.e.
// gravity pointing in the direction of the hinge). When this happens abrupt
// small acceleration perpendicular to the hinge can lead to incorrect hinge
// angle calculations. To prevent this the accelerometer updates will be
// smoothed over time in order to reduce this noise.
// This is the minimum acceleration parallel to the hinge under which to begin
// smoothing in m/s^2.
constexpr float kHingeVerticalSmoothingStart = 7.0f;
// This is the maximum acceleration parallel to the hinge under which smoothing
// will incorporate new acceleration values, in m/s^2.
constexpr float kHingeVerticalSmoothingMaximum = 8.7f;

// The maximum deviation between the magnitude of the two accelerometers under
// which to detect hinge angle in m/s^2. These accelerometers are attached to
// the same physical device and so should be under the same acceleration.
constexpr float kNoisyMagnitudeDeviation = 1.0f;

// Interval between calls to RecordLidAngle().
constexpr base::TimeDelta kRecordLidAngleInterval = base::Hours(1);

// Time that should wait to reset |occlusion_tracker_pauser_| on
// entering/exiting tablet mode.
constexpr base::TimeDelta kOcclusionTrackerTimeout = base::Seconds(1);

// Histogram names for recording animation smoothness when entering or exiting
// tablet mode.
constexpr char kTabletModeEnterHistogram[] =
    "Ash.TabletMode.AnimationSmoothness.Enter";
constexpr char kTabletModeExitHistogram[] =
    "Ash.TabletMode.AnimationSmoothness.Exit";

// Set to false for tests so tablet mode can be changed synchronously.
bool use_screenshot_for_test = true;

// The angle between AccelerometerReadings are considered stable if
// their magnitudes do not differ greatly. This returns false if the deviation
// between the screen and keyboard accelerometers is too high.
bool IsAngleBetweenAccelerometerReadingsStable(
    const AccelerometerUpdate& update) {
  return std::abs(
             update.GetVector(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD).Length() -
             update.GetVector(ACCELEROMETER_SOURCE_SCREEN).Length()) <=
         kNoisyMagnitudeDeviation;
}

// Returns the UiMode given by the force-table-mode command line.
TabletModeController::UiMode GetUiMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshUiMode)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kAshUiMode);
    if (switch_value == switches::kAshUiModeClamshell) {
      return TabletModeController::UiMode::kClamshell;
    }

    if (switch_value == switches::kAshUiModeTablet) {
      return TabletModeController::UiMode::kTabletMode;
    }
  }
  return TabletModeController::UiMode::kNone;
}

// Returns true if the device has an active internal display.
bool HasActiveInternalDisplay() {
  if (!display::HasInternalDisplay()) {
    return false;
  }

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  return display_manager->IsActiveDisplayId(
             display::Display::InternalDisplayId()) ||
         display_manager->IsInUnifiedMode();
}

// Returns true if |sequence| has the same properties as the ones we care about
// for the tablet transition animation.
bool ShouldObserveSequence(ui::LayerAnimationSequence* sequence) {
  DCHECK(sequence);
  return sequence->properties() &
         TabletModeController::GetObservedTabletTransitionProperty();
}

// Check if there is any external and internal pointing device in
// |input_devices|.
template <typename PointingDeviceType>
void CheckHasPointingDevices(
    const std::vector<PointingDeviceType>& input_devices,
    BluetoothDevicesObserver* bluetooth_device_observer,
    bool& out_has_external_pointing_device,
    bool& out_has_internal_pointing_device) {
  static_assert(std::is_base_of<ui::InputDevice, PointingDeviceType>::value);
  for (const PointingDeviceType& input_device : input_devices) {
    if (input_device.type == ui::INPUT_DEVICE_INTERNAL) {
      out_has_internal_pointing_device = true;
    } else if (input_device.type == ui::INPUT_DEVICE_USB ||
               (input_device.type == ui::INPUT_DEVICE_BLUETOOTH &&
                bluetooth_device_observer->IsConnectedBluetoothDevice(
                    input_device))) {
      out_has_external_pointing_device = true;
    }
    if (out_has_external_pointing_device && out_has_internal_pointing_device) {
      return;
    }
  }
}

// The default behavior in Clamshell mode.
constexpr TabletModeController::TabletModeBehavior kDefault{};

// Defines the behavior of the tablet mode when enabled by sensor.
constexpr TabletModeController::TabletModeBehavior kOnBySensor{
    .block_internal_input_device = true,
};

// Defines the behavior that sticks to tablet mode. Used to implement the
// --force-tablet-mode=touch_view flag.
constexpr TabletModeController::TabletModeBehavior kLockInTabletMode{
    .use_sensor = false,
    .observe_display_events = false,
    .observe_pointer_device_events = false,
    .always_show_overview_button = true,
};

// Defines the behavior that sticks to tablet mode. Used to implement the
// --force-tablet-mode=clamshell flag.
constexpr TabletModeController::TabletModeBehavior kLockInClamshellMode{
    .use_sensor = false,
    .observe_display_events = false,
    .observe_pointer_device_events = false,
};

// Defines the behavior used for testing. It prevents the device from
// switching the mode due to sensor events during the test.
constexpr TabletModeController::TabletModeBehavior kOnForTest{
    .use_sensor = false,
    .block_internal_input_device = true,
    .force_physical_tablet_state =
        TabletModeController::ForcePhysicalTabletState::kForceTabletMode,
};

// Used for the testing API to forcibly enter into the tablet mode. It should
// not observe hardware events as tests want to stick with the tablet mode, and
// it should not block internal keyboard as some tests may want to use keyboard
// events in the tablet mode.
constexpr TabletModeController::TabletModeBehavior kOnForAutotest{
    .use_sensor = false,
    .observe_display_events = false,
    .observe_pointer_device_events = false,
    .force_physical_tablet_state =
        TabletModeController::ForcePhysicalTabletState::kForceTabletMode,
};

// Used for the testing API to forcibly exit from the tablet mode.
constexpr TabletModeController::TabletModeBehavior kOffForAutotest{
    .use_sensor = false,
    .observe_display_events = false,
    .observe_pointer_device_events = false,
    .force_physical_tablet_state =
        TabletModeController::ForcePhysicalTabletState::kForceClamshellMode,
};

// Used for development purpose (currently debug shortcut shift-ctrl-alt). This
// ignores the sensor but allows to switch upon docked mode and external
// pointing device. It also forces to show the overview button.
constexpr TabletModeController::TabletModeBehavior kOnForDev{
    .use_sensor = false,
    .always_show_overview_button = true,
    .force_physical_tablet_state =
        TabletModeController::ForcePhysicalTabletState::kForceTabletMode,
};

using LidState = chromeos::PowerManagerClient::LidState;
using TabletMode = chromeos::PowerManagerClient::TabletMode;

const char* ToString(LidState lid_state) {
  switch (lid_state) {
    case LidState::OPEN:
      return "Open";
    case LidState::CLOSED:
      return "Closed";
    case LidState::NOT_PRESENT:
      return "Not present";
  }
}

const char* ToString(TabletMode tablet_mode) {
  switch (tablet_mode) {
    case TabletMode::ON:
      return "On";
    case TabletMode::OFF:
      return "Off";
    case TabletMode::UNSUPPORTED:
      return "Unsupported";
  }
}

void ReportTrasitionSmoothness(bool enter_tablet_mode, int smoothness) {
  if (enter_tablet_mode) {
    UMA_HISTOGRAM_PERCENTAGE(kTabletModeEnterHistogram, smoothness);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(kTabletModeExitHistogram, smoothness);
  }
}

bool ShouldBlockUiTabletModeInKiosk() {
  return base::FeatureList::IsEnabled(kBlockUiTabletModeInKiosk);
}

}  // namespace

// An observer that observes the destruction of the |window_| and executes the
// callback. Used to run cleanup when the window is destroyed in the middle of
// certain operation.
class TabletModeController::DestroyObserver : public aura::WindowObserver {
 public:
  DestroyObserver(aura::Window* window, base::OnceCallback<void(void)> callback)
      : window_(window), callback_(std::move(callback)) {
    window_->AddObserver(this);
  }
  ~DestroyObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
    }
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
    std::move(callback_).Run();
  }

  aura::Window* window() { return window_; }

 private:
  raw_ptr<aura::Window> window_;
  base::OnceCallback<void(void)> callback_;
};

// Used to hide the shelf and float containers while screenshot for tablet mode
// animation is taken.
class TabletModeController::ScopedContainerHider {
 public:
  explicit ScopedContainerHider(aura::Window* root_window)
      : root_window_(root_window) {
    DCHECK(root_window->IsRootWindow());

    ui::Layer* screen_animation_container_layer =
        root_window->GetChildById(kShellWindowId_ScreenAnimationContainer)
            ->layer();
    for (int id :
         {kShellWindowId_FloatContainer, kShellWindowId_ShelfContainer}) {
      aura::Window* container = root_window->GetChildById(id);

      std::unique_ptr<ui::LayerTreeOwner> phantom_layer =
          wm::RecreateLayers(container);
      ui::Layer* root = phantom_layer->root();
      root_window->layer()->Add(root);
      root_window->layer()->StackAbove(root, screen_animation_container_layer);
      phantom_layers_.push_back(std::move(phantom_layer));

      container->layer()->SetOpacity(0.0f);
    }
  }
  ScopedContainerHider(const ScopedContainerHider&) = delete;
  ScopedContainerHider& operator=(const ScopedContainerHider&) = delete;
  ~ScopedContainerHider() {
    // Cancel if the root window is deleted while taking a screenshot.
    if (!base::Contains(Shell::GetAllRootWindows(), root_window_)) {
      return;
    }

    for (int id :
         {kShellWindowId_FloatContainer, kShellWindowId_ShelfContainer}) {
      root_window_->GetChildById(id)->layer()->SetOpacity(1.0f);
    }
  }

 private:
  const raw_ptr<aura::Window> root_window_;

  // The layer that holds the clone of shelf and float layers while the
  // originals are hidden.
  std::vector<std::unique_ptr<ui::LayerTreeOwner>> phantom_layers_;
};

constexpr char TabletModeController::kLidAngleHistogramName[];
constexpr char TabletModeController::kTabletInactiveTimeHistogramName[];
constexpr char TabletModeController::kTabletActiveTimeHistogramName[];

////////////////////////////////////////////////////////////////////////////////
// TabletModeController, public:

// static
void TabletModeController::SetUseScreenshotForTest(bool use_screenshot) {
  use_screenshot_for_test = use_screenshot;
}

// static
ui::LayerAnimationElement::AnimatableProperty
TabletModeController::GetObservedTabletTransitionProperty() {
  return ui::LayerAnimationElement::TRANSFORM;
}

TabletModeController::TabletModeController()
    : event_blocker_(std::make_unique<InternalInputDevicesEventBlocker>()),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  Shell::Get()->AddShellObserver(this);
  base::RecordAction(base::UserMetricsAction("Touchview_Initially_Disabled"));

  // TODO(jonross): Do not create TabletModeController if the flag is
  // unavailable. This will require refactoring
  // InTabletMode to check for the existence of the
  // controller.
  if (IsBoardTypeMarkedAsTabletCapable()) {
    Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
    AccelerometerReader::GetInstance()->AddObserver(this);
    ui::DeviceDataManager::GetInstance()->AddObserver(this);
    bluetooth_devices_observer_ =
        std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
            &TabletModeController::OnBluetoothAdapterOrDeviceChanged,
            base::Unretained(this)));
  }

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  power_manager_client->AddObserver(this);
}

TabletModeController::~TabletModeController() {
  DCHECK(!tablet_mode_window_manager_);
}

void TabletModeController::Shutdown() {
  // Stop observing any animations and delete any pending screenshots.
  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/true);

  if (tablet_mode_window_manager_) {
    tablet_mode_window_manager_->Shutdown(
        TabletModeWindowManager::ShutdownReason::kSystemShutdown);
  }
  tablet_mode_window_manager_.reset();

  UMA_HISTOGRAM_COUNTS_1000("Tablet.AppWindowDrag.CountOfPerUserSession",
                            app_window_drag_count_);
  UMA_HISTOGRAM_COUNTS_1000(
      "Tablet.AppWindowDrag.InSplitView.CountOfPerUserSession",
      app_window_drag_in_splitview_count_);
  UMA_HISTOGRAM_COUNTS_1000("Tablet.TabDrag.CountOfPerUserSession",
                            tab_drag_count_);
  UMA_HISTOGRAM_COUNTS_1000("Tablet.TabDrag.InSplitView.CountOfPerUserSession",
                            tab_drag_in_splitview_count_);

  Shell::Get()->RemoveShellObserver(this);

  if (IsBoardTypeMarkedAsTabletCapable()) {
    Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
    AccelerometerReader::GetInstance()->RemoveObserver(this);
    ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  }
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);

  for (auto& observer : tablet_mode_observers_) {
    observer.OnTabletControllerDestroyed();
  }
}

void TabletModeController::AddWindow(aura::Window* window) {
  if (tablet_mode_window_manager_) {
    tablet_mode_window_manager_->AddWindow(window);
  }
}

bool TabletModeController::ShouldAutoHideTitlebars(views::Widget* widget) {
  DCHECK(widget);
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }
  return widget->IsMaximized() ||
         (WindowState::Get(widget->GetNativeWindow()) &&
          WindowState::Get(widget->GetNativeWindow())->IsSnapped());
}

bool TabletModeController::AreInternalInputDeviceEventsBlocked() const {
  return event_blocker_->should_be_blocked();
}

bool TabletModeController::TriggerRecordLidAngleTimerForTesting() {
  if (!record_lid_angle_timer_.IsRunning()) {
    return false;
  }

  record_lid_angle_timer_.user_task().Run();
  return true;
}

void TabletModeController::MaybeObserveBoundsAnimation(aura::Window* window) {
  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/false);

  if (!display::IsTabletStateChanging(
          display::Screen::GetScreen()->GetTabletState())) {
    return;
  }

  destroy_observer_ = std::make_unique<DestroyObserver>(
      window, base::BindOnce(&TabletModeController::StopObservingAnimation,
                             weak_factory_.GetWeakPtr(),
                             /*record_stats=*/false,
                             /*delete_screenshot=*/true));
  animating_layer_ = window->layer();
  animating_layer_->GetAnimator()->AddObserver(this);
  animating_layer_->AddObserver(this);
}

void TabletModeController::StopObservingAnimation(bool record_stats,
                                                  bool delete_screenshot) {
  StopObserving();

  ResetDestroyObserver();

  if (animating_layer_) {
    animating_layer_->GetAnimator()->StopAnimating();

    // If the observed layer is part of a cross fade animation, stopping the
    // animation will end up destroying the layer.
    if (animating_layer_) {
      animating_layer_->RemoveObserver(this);
      animating_layer_->GetAnimator()->RemoveObserver(this);
      animating_layer_ = nullptr;
    }
  }

  if (record_stats && transition_tracker_) {
    transition_tracker_->Stop();
  }
  transition_tracker_.reset();

  // Stop other animations (STEP_END), then update the tablet mode ui.
  if (tablet_mode_window_manager_ && delete_screenshot) {
    tablet_mode_window_manager_->StopWindowAnimations();
  }

  if (delete_screenshot) {
    DeleteScreenshot();
  }
}

bool TabletModeController::IsInDevTabletMode() const {
  return tablet_mode_behavior_ == kOnForDev;
}

void TabletModeController::AddObserver(TabletModeObserver* observer) {
  tablet_mode_observers_.AddObserver(observer);
}

void TabletModeController::RemoveObserver(TabletModeObserver* observer) {
  tablet_mode_observers_.RemoveObserver(observer);
}

bool TabletModeController::ForceUiTabletModeState(std::optional<bool> enabled) {
  if (!enabled.has_value()) {
    tablet_mode_behavior_ = kDefault;
  } else if (*enabled) {
    tablet_mode_behavior_ = kOnForAutotest;
  } else {
    tablet_mode_behavior_ = kOffForAutotest;
  }

  // We want to suppress the accelerometer to auto-rotate the screen based on
  // the physical orientation, as it will confuse the test scenarios. Note that
  // this should not block ScreenOrientationController as the screen may want
  // to be rotated for other factors.
  AccelerometerReader::GetInstance()->SetEnabled(!enabled.has_value());
  return SetIsInTabletPhysicalState(CalculateIsInTabletPhysicalState()) ||
         UpdateUiTabletState();
}

void TabletModeController::SetEnabledForTest(bool enabled) {
  tablet_mode_behavior_ = enabled ? kOnForTest : kDefault;

  SetIsInTabletPhysicalState(enabled);
}

void TabletModeController::OnShellInitialized() {
  forced_ui_mode_ = GetUiMode();
  switch (forced_ui_mode_) {
    case UiMode::kTabletMode:
      tablet_mode_behavior_ = kLockInTabletMode;
      UpdateUiTabletState();
      break;

    case UiMode::kClamshell:
      tablet_mode_behavior_ = kLockInClamshellMode;
      UpdateUiTabletState();
      break;

    case UiMode::kNone:
      break;
  }
}

void TabletModeController::OnDidApplyDisplayChanges() {
  if (!tablet_mode_behavior_.observe_display_events) {
    return;
  }

  // Display config changes might be due to entering or exiting docked mode, in
  // which case the availability of an active internal display changes.
  // Therefore we update the physical tablet state of the device.
  SetIsInTabletPhysicalState(CalculateIsInTabletPhysicalState());
}

void TabletModeController::OnLoginStatusChanged(LoginStatus login_status) {
  if (login_status == LoginStatus::KIOSK_APP &&
      ShouldBlockUiTabletModeInKiosk()) {
    // Tablet mode is not allowed during the kiosk session. No need to reset the
    // `forced_ui_mode_` to the previous state because on kiosk exit Chrome
    // restarts, so the `TabletModeController` will be reset.`
    // If the device is currently in the UI tablet mode, it will be forced to
    // switch to the clamshell UI mode.
    forced_ui_mode_ = UiMode::kClamshell;
    UpdateUiTabletState();
  }
}

void TabletModeController::OnChromeTerminating() {
  // The system is about to shut down, so record TabletMode usage interval
  // metrics based on whether TabletMode mode is currently active.
  RecordTabletModeUsageInterval(CurrentTabletModeIntervalType());

  // Only when |tablet_mode_usage_interval_start_time_| is not null,
  // |total_tablet_mode_time_| and |total_non_tablet_mode_time_| will have valid
  // values.
  if (!tablet_mode_usage_interval_start_time_.is_null()) {
    DCHECK(CanEnterTabletMode() && initial_input_device_set_up_finished_ &&
           have_seen_tablet_mode_event_);

    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewActiveTotal",
                                total_tablet_mode_time_.InMinutes(), 1,
                                base::Days(7).InMinutes(), 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewInactiveTotal",
                                total_non_tablet_mode_time_.InMinutes(), 1,
                                base::Days(7).InMinutes(), 50);
    base::TimeDelta total_runtime =
        total_tablet_mode_time_ + total_non_tablet_mode_time_;
    if (total_runtime.InSeconds() > 0) {
      UMA_HISTOGRAM_PERCENTAGE("Ash.TouchView.TouchViewActivePercentage",
                               100 * total_tablet_mode_time_.InSeconds() /
                                   total_runtime.InSeconds());
    }
  }
}

void TabletModeController::OnECLidAngleDriverStatusChanged(bool is_supported) {
  is_ec_lid_angle_driver_supported_ = is_supported;

  // OnECLidAngleDriverStatusChanged is guaranteed to be called before
  // OnAccelerometerUpdated. Thus calling
  // StartTrackingTabletUsageMetricsIfApplicable() before or after
  // `!is_supported` won't make any difference. The reason is that for
  // `!is_supported` case, because we haven't seen any accelerometer data yet,
  // we won't start logging here anyway.
  // OnECLidAngleDriverStatusChanged can be called before or after
  // TabletModeEventReceived. Thus we'll need the logging both here and in
  // TabletModeEventReceived function.
  StartTrackingTabletUsageMetricsIfApplicable();

  if (!is_supported) {
    return;
  }

  // When ChromeOS EC lid angle driver is supported, EC can handle lid angle
  // calculation, thus Chrome side lid angle calculation is disabled. In this
  // case, TabletModeController no longer listens to accelerometer samples.

  // Reset lid angle that might be calculated before lid angle driver is
  // read.
  lid_angle_ = 0.f;
  can_detect_lid_angle_ = false;
  if (record_lid_angle_timer_.IsRunning()) {
    record_lid_angle_timer_.Stop();
  }
  AccelerometerReader::GetInstance()->RemoveObserver(this);
}

void TabletModeController::OnAccelerometerUpdated(
    const AccelerometerUpdate& update) {
  have_seen_accelerometer_data_ = true;
  can_detect_lid_angle_ = update.has(ACCELEROMETER_SOURCE_SCREEN) &&
                          update.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
  if (!can_detect_lid_angle_) {
    if (record_lid_angle_timer_.IsRunning()) {
      record_lid_angle_timer_.Stop();
    }
  } else if (HasActiveInternalDisplay() && tablet_mode_behavior_.use_sensor) {
    // Whether or not we enter tablet mode affects whether we handle screen
    // rotation, so determine whether to enter tablet mode first.
    if (update.IsReadingStable(ACCELEROMETER_SOURCE_SCREEN) &&
        update.IsReadingStable(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD) &&
        IsAngleBetweenAccelerometerReadingsStable(update)) {
      // update.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)
      // Ignore the reading if it appears unstable. The reading is considered
      // unstable if it deviates too much from gravity and/or the magnitude of
      // the reading from the lid differs too much from the reading from the
      // base.
      HandleHingeRotation(update);
    }
  }

  StartTrackingTabletUsageMetricsIfApplicable();
}

void TabletModeController::PowerManagerBecameAvailable(bool available) {
  if (!available) {
    return;
  }
  chromeos::PowerManagerClient::Get()->GetSwitchStates(base::BindOnce(
      &TabletModeController::OnGetSwitchStates, weak_factory_.GetWeakPtr()));
}

void TabletModeController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks time) {
  VLOG(1) << "Lid event received: " << ToString(state);
  lid_is_closed_ = state != chromeos::PowerManagerClient::LidState::OPEN;
  if (lid_is_closed_) {
    // Reset |lid_angle_| to 0.f when lid is closed. The accelerometer readings
    // can be wrong when lid is closed, e.g., it can report lid angle to be
    // around 360 degrees when lid is nearly closed.
    lid_angle_ = 0.f;
  }

  if (!tablet_mode_behavior_.use_sensor) {
    return;
  }

  SetIsInTabletPhysicalState(CalculateIsInTabletPhysicalState());
}

void TabletModeController::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    base::TimeTicks time) {
  have_seen_tablet_mode_event_ = true;
  if (tablet_mode_behavior_.use_sensor) {
    VLOG(1) << "Tablet mode event received: " << ToString(mode);
    const bool on = mode == chromeos::PowerManagerClient::TabletMode::ON;

    tablet_mode_switch_is_on_ = on;
    tablet_mode_behavior_ = on ? kOnBySensor : kDefault;

    SetIsInTabletPhysicalState(CalculateIsInTabletPhysicalState());
  }

  StartTrackingTabletUsageMetricsIfApplicable();
}

void TabletModeController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  // The system is about to suspend, so record TabletMode usage interval metrics
  // based on whether TabletMode mode is currently active.
  RecordTabletModeUsageInterval(CurrentTabletModeIntervalType());

  // Stop listening to any incoming input device changes during suspend as the
  // input devices may be removed during suspend and cause the device enter/exit
  // tablet mode unexpectedly.
  if (IsBoardTypeMarkedAsTabletCapable()) {
    ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
    bluetooth_devices_observer_.reset();
  }
}

void TabletModeController::SuspendDone(base::TimeDelta sleep_duration) {
  // We do not want TabletMode usage metrics to include time spent in suspend.
  if (!tablet_mode_usage_interval_start_time_.is_null()) {
    tablet_mode_usage_interval_start_time_ = base::Time::Now();
  }

  // Start listening to the input device changes again.
  // It might be possible that the suspend request has been cancelled so
  // `this` was not removed as an observer of the input device changes. See
  // b/271634754 for details.
  auto* device_data_manager = ui::DeviceDataManager::GetInstance();
  if (IsBoardTypeMarkedAsTabletCapable() &&
      !device_data_manager->HasObserver(this)) {
    bluetooth_devices_observer_ =
        std::make_unique<BluetoothDevicesObserver>(base::BindRepeating(
            &TabletModeController::OnBluetoothAdapterOrDeviceChanged,
            base::Unretained(this)));
    device_data_manager->AddObserver(this);
    // Call HandlePointingDeviceAddedOrRemoved() to iterate all available input
    // devices just in case we have missed all the notifications from
    // DeviceDataManager and  BluetoothDevicesObserver when SuspendDone() is
    // called.
    HandlePointingDeviceAddedOrRemoved();
  }
}

void TabletModeController::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & (ui::InputDeviceEventObserver::kMouse |
                            ui::InputDeviceEventObserver::kTouchpad |
                            ui::InputDeviceEventObserver::kPointingStick)) {
    if (input_device_types & ui::InputDeviceEventObserver::kMouse) {
      VLOG(1) << "Mouse device configuration changed.";
    }
    if (input_device_types & ui::InputDeviceEventObserver::kTouchpad) {
      VLOG(1) << "Touchpad device configuration changed.";
    }
    if (input_device_types & ui::InputDeviceEventObserver::kPointingStick) {
      VLOG(1) << "Pointing stick device configuration changed.";
    }

    HandlePointingDeviceAddedOrRemoved();
  }
}

void TabletModeController::OnDeviceListsComplete() {
  initial_input_device_set_up_finished_ = true;
  HandlePointingDeviceAddedOrRemoved();

  StartTrackingTabletUsageMetricsIfApplicable();
}

void TabletModeController::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {}

void TabletModeController::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  if (!transition_tracker_ || !ShouldObserveSequence(sequence)) {
    return;
  }

  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/true);
}

void TabletModeController::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  // This may be called before |OnLayerAnimationScheduled()| if tablet is
  // entered/exited while an animation is in progress, so we won't get
  // stats/screenshot in those cases.
  // TODO(sammiequon): We may want to remove the |transition_tracker_| check and
  // simplify things since those are edge cases.
  if (!transition_tracker_ || !ShouldObserveSequence(sequence)) {
    return;
  }

  StopObservingAnimation(/*record_stats=*/true, /*delete_screenshot=*/true);
}

void TabletModeController::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  if (!ShouldObserveSequence(sequence)) {
    return;
  }

  if (!transition_tracker_) {
    transition_tracker_ =
        animating_layer_->GetCompositor()->RequestNewThroughputTracker();
    transition_tracker_->Start(metrics_util::ForSmoothnessV3(
        base::BindRepeating(&ReportTrasitionSmoothness,
                            display::Screen::GetScreen()->GetTabletState() ==
                                display::TabletState::kEnteringTabletMode)));
    return;
  }

  // If another animation is scheduled while the animation we were originally
  // watching is still animating, abort and do not log stats as the stats will
  // not be accurate.
  StopObservingAnimation(/*record_stats=*/false, /*delete_screenshot=*/true);
}

void TabletModeController::LayerDestroyed(ui::Layer* layer) {
  DCHECK_EQ(animating_layer_, layer);
  animating_layer_->RemoveObserver(this);
  animating_layer_->GetAnimator()->RemoveObserver(this);
  animating_layer_ = nullptr;
}

void TabletModeController::SetEnabledForDev(bool enabled) {
  tablet_mode_behavior_ = enabled ? kOnForDev : kDefault;
  force_notify_events_blocking_changed_ = true;

  SetIsInTabletPhysicalState(enabled);
}

bool TabletModeController::ShouldShowOverviewButton() const {
  return AreInternalInputDeviceEventsBlocked() ||
         tablet_mode_behavior_.always_show_overview_button;
}

bool TabletModeController::CanEnterTabletMode() const {
  // If ChromeOS EC lid angle driver is supported, EC can handle lid angle
  // calculation, and trigger tablet mode at some point.
  // Otherwise, lid angle calculation is done on Chrome side for convertible
  // device. If we have ever seen accelerometer data, then HandleHingeRotation
  // may trigger tablet mode at some point in the future.
  return IsBoardTypeMarkedAsTabletCapable() &&
         (is_ec_lid_angle_driver_supported_.value_or(false) ||
          have_seen_accelerometer_data_);
}

////////////////////////////////////////////////////////////////////////////////
// TabletModeController, private:

void TabletModeController::SetTabletModeEnabledInternal(bool should_enable) {
  DCHECK_NE(display::Screen::GetScreen()->InTabletMode(), should_enable);

  // Hide the context menu on entering tablet mode to prevent users from
  // accessing forbidden options. Hide the context menu on exiting tablet mode
  // to match behaviors.
  HideActiveContextMenu();

  // Suspend occlusion tracker when entering or exiting tablet mode.
  SuspendOcclusionTracker();
  DeleteScreenshot();

  if (should_enable) {
    Shell::Get()->display_manager()->SetTabletState(
        display::TabletState::kEnteringTabletMode);

    // Take a screenshot if there is a top window that will get animated.
    // Floated windows will always get animated, and if the only window is a
    // floated window, we don't take a screenshot since the floated window in
    // tablet mode does not cover the whole work area.
    // TODO(sammiequon): Handle the case where the top window is not on the
    // primary display.
    aura::Window* top_window = window_util::GetTopNonFloatedWindow();
    const bool top_window_on_primary_display =
        top_window &&
        top_window->GetRootWindow() == Shell::GetPrimaryRootWindow();
    // If the top window was already animating (eg. tablet mode event received
    // while create window animation still running), skip taking the screenshot.
    // It will take a performance hit but will remove cases where the screenshot
    // might not get deleted because of the extra animation observer methods
    // getting fired.
    const bool top_window_animating =
        top_window && top_window->layer()->GetAnimator()->is_animating();
    // We'll keep overview active after clamshell <-> tablet mode transition if
    // it was active before transition, do not take screenshot if overview is
    // active in this case.
    const bool overview_remain_active =
        Shell::Get()->overview_controller()->InOverviewSession();
    if (use_screenshot_for_test && top_window_on_primary_display &&
        !top_window_animating && !overview_remain_active) {
      TakeScreenshot(top_window);
    } else {
      FinishInitTabletMode();
    }
  } else {
    // We may have entered tablet mode, then tried to exit before the screenshot
    // was taken. In this case `tablet_mode_window_manager_` will be null.
    if (tablet_mode_window_manager_) {
      tablet_mode_window_manager_->SetIgnoreWmEventsForExit();
    }

    Shell::Get()->display_manager()->SetTabletState(
        display::TabletState::kExitingTabletMode);

    // Many events can lead to shelf config updates as a result of
    // kInClamshellMode event. Update the shelf config during "ending"
    // stage rather than the "ended", so `in_tablet_mode_` gets updated
    // correctly, and the shelf bounds are stabilized early so as not to have
    // multiple unnecessary work-area bounds changes.
    ShelfConfig::Get()->UpdateForTabletMode(/*in_tablet_mode=*/false);

    if (tablet_mode_window_manager_) {
      tablet_mode_window_manager_->Shutdown(
          TabletModeWindowManager::ShutdownReason::kExitTabletUIMode);
    }
    tablet_mode_window_manager_.reset();

    base::RecordAction(base::UserMetricsAction("Touchview_Disabled"));
    RecordTabletModeUsageInterval(TABLET_MODE_INTERVAL_ACTIVE);
    Shell::Get()->display_manager()->SetTabletState(
        display::TabletState::kInClamshellMode);
    VLOG(1) << "Exit tablet mode.";

    UpdateInternalInputDevicesEventBlocker();
    Shell::Get()->cursor_manager()->ShowCursor();
  }
}

void TabletModeController::HandleHingeRotation(
    const AccelerometerUpdate& update) {
  static const gfx::Vector3dF hinge_vector(1.0f, 0.0f, 0.0f);
  gfx::Vector3dF base_reading =
      update.GetVector(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
  gfx::Vector3dF lid_reading = update.GetVector(ACCELEROMETER_SOURCE_SCREEN);

  // As the hinge approaches a vertical angle, the base and lid accelerometers
  // approach the same values making any angle calculations highly inaccurate.
  // Smooth out instantaneous acceleration when nearly vertical to increase
  // accuracy.
  float largest_hinge_acceleration =
      std::max(std::abs(base_reading.x()), std::abs(lid_reading.x()));
  float smoothing_ratio = std::clamp(
      (largest_hinge_acceleration - kHingeVerticalSmoothingStart) /
          (kHingeVerticalSmoothingMaximum - kHingeVerticalSmoothingStart),
      0.0f, 1.0f);

  // We cannot trust the computed lid angle when the device is held vertically.
  bool is_angle_reliable =
      largest_hinge_acceleration <= kHingeVerticalSmoothingMaximum;

  base_smoothed_.Scale(smoothing_ratio);
  base_reading.Scale(1.0f - smoothing_ratio);
  base_smoothed_.Add(base_reading);

  lid_smoothed_.Scale(smoothing_ratio);
  lid_reading.Scale(1.0f - smoothing_ratio);
  lid_smoothed_.Add(lid_reading);

  if (tablet_mode_switch_is_on_) {
    return;
  }

  // Do not calculate lid angle when lid is closed to prevent the device
  // accidentally entering tablet mode. The angle calculated when lid is closed
  // is not accurate (the angle between the base and the lid might be a minus
  // value when lid is closed, and since we do adjustment for minus values, the
  // angle might be in the same range as tablet mode angle range).
  if (lid_is_closed_) {
    return;
  }

  // Ignore the component of acceleration parallel to the hinge for the purposes
  // of hinge angle calculation.
  gfx::Vector3dF base_flattened(base_smoothed_);
  gfx::Vector3dF lid_flattened(lid_smoothed_);
  base_flattened.set_x(0.0f);
  lid_flattened.set_x(0.0f);

  // Compute the angle between the base and the lid.
  lid_angle_ = 180.0f - gfx::ClockwiseAngleBetweenVectorsInDegrees(
                            base_flattened, lid_flattened, hinge_vector);
  if (lid_angle_ < 0.0f) {
    lid_angle_ += 360.0f;
  }

  lid_angle_is_stable_ = is_angle_reliable && lid_angle_ >= kMinStableAngle &&
                         lid_angle_ <= kMaxStableAngle;

  if (lid_angle_is_stable_) {
    // Reset the timestamp of first unstable lid angle because we get a stable
    // reading.
    first_unstable_lid_angle_time_ = base::TimeTicks();
  } else if (first_unstable_lid_angle_time_.is_null()) {
    first_unstable_lid_angle_time_ = tick_clock_->NowTicks();
  }

  const bool new_tablet_physical_state = CalculateIsInTabletPhysicalState();
  tablet_mode_behavior_ = new_tablet_physical_state ? kOnBySensor : kDefault;
  SetIsInTabletPhysicalState(new_tablet_physical_state);

  // Start reporting the lid angle if we aren't already doing so.
  if (!record_lid_angle_timer_.IsRunning()) {
    record_lid_angle_timer_.Start(
        FROM_HERE, kRecordLidAngleInterval,
        base::BindRepeating(&TabletModeController::RecordLidAngle,
                            base::Unretained(this)));
  }
}

void TabletModeController::OnGetSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> result) {
  if (!result.has_value()) {
    return;
  }

  LidEventReceived(result->lid_state, base::TimeTicks::Now());
  TabletModeEventReceived(result->tablet_mode, base::TimeTicks::Now());
}

bool TabletModeController::CanUseUnstableLidAngle() const {
  DCHECK(!first_unstable_lid_angle_time_.is_null());

  const base::TimeTicks now = tick_clock_->NowTicks();
  DCHECK(now >= first_unstable_lid_angle_time_);
  const base::TimeDelta elapsed_time = now - first_unstable_lid_angle_time_;
  return elapsed_time >= kUnstableLidAngleDuration;
}

void TabletModeController::RecordTabletModeUsageInterval(
    TabletModeIntervalType type) {
  // If |tablet_mode_usage_interval_start_time_| is null, do not record any
  // tablet mode usage metrics. It may happen when we have some false positive
  // tablet mode activations during startup.
  if (tablet_mode_usage_interval_start_time_.is_null()) {
    return;
  }

  DCHECK(CanEnterTabletMode() && initial_input_device_set_up_finished_ &&
         have_seen_tablet_mode_event_);

  base::Time current_time = base::Time::Now();
  base::TimeDelta delta = current_time - tablet_mode_usage_interval_start_time_;
  switch (type) {
    case TABLET_MODE_INTERVAL_INACTIVE:
      UMA_HISTOGRAM_LONG_TIMES(kTabletInactiveTimeHistogramName, delta);
      total_non_tablet_mode_time_ += delta;
      break;
    case TABLET_MODE_INTERVAL_ACTIVE:
      UMA_HISTOGRAM_LONG_TIMES(kTabletActiveTimeHistogramName, delta);
      total_tablet_mode_time_ += delta;
      break;
  }

  tablet_mode_usage_interval_start_time_ = current_time;
}

void TabletModeController::RecordLidAngle() {
  DCHECK(can_detect_lid_angle_);
  base::LinearHistogram::FactoryGet(
      kLidAngleHistogramName, /*minimum=*/1, /*maximum=*/360,
      /*bucket_count=*/50, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(std::round(lid_angle_));
}

TabletModeController::TabletModeIntervalType
TabletModeController::CurrentTabletModeIntervalType() {
  return display::Screen::GetScreen()->InTabletMode()
             ? TABLET_MODE_INTERVAL_ACTIVE
             : TABLET_MODE_INTERVAL_INACTIVE;
}

void TabletModeController::HandlePointingDeviceAddedOrRemoved() {
  if (!initial_input_device_set_up_finished_) {
    return;
  }

  bool has_external_pointing_device = false;
  bool has_internal_pointing_device = false;

  // Check if there is an external and internal mouse or touchpad device.
  CheckHasPointingDevices(
      ui::DeviceDataManager::GetInstance()->GetMouseDevices(),
      bluetooth_devices_observer_.get(), has_external_pointing_device,
      has_internal_pointing_device);
  if (!has_external_pointing_device || !has_internal_pointing_device) {
    CheckHasPointingDevices(
        ui::DeviceDataManager::GetInstance()->GetTouchpadDevices(),
        bluetooth_devices_observer_.get(), has_external_pointing_device,
        has_internal_pointing_device);
  }
  if (!has_external_pointing_device || !has_internal_pointing_device) {
    CheckHasPointingDevices(
        ui::DeviceDataManager::GetInstance()->GetPointingStickDevices(),
        bluetooth_devices_observer_.get(), has_external_pointing_device,
        has_internal_pointing_device);
  }

  const bool changed =
      (has_external_pointing_device_ != has_external_pointing_device) ||
      (has_internal_pointing_device_ != has_internal_pointing_device);

  if (!changed) {
    return;
  }

  has_external_pointing_device_ = has_external_pointing_device;
  has_internal_pointing_device_ = has_internal_pointing_device;

  // We only need to update UI state if observed internal pointing device or
  // external pointing device changed.
  if (tablet_mode_behavior_.observe_pointer_device_events) {
    UpdateUiTabletState();
  }
}

void TabletModeController::OnBluetoothAdapterOrDeviceChanged(
    device::BluetoothDevice* device) {
  // We only care about pointing type bluetooth device change. Note KEYBOARD
  // type is also included here as sometimes a bluetooth keyboard comes with a
  // touch pad.
  if (!device ||
      device->GetDeviceType() == device::BluetoothDeviceType::MOUSE ||
      device->GetDeviceType() ==
          device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO ||
      device->GetDeviceType() == device::BluetoothDeviceType::KEYBOARD ||
      device->GetDeviceType() == device::BluetoothDeviceType::TABLET) {
    VLOG(1) << "Bluetooth device configuration changed.";
    HandlePointingDeviceAddedOrRemoved();
  }
}

void TabletModeController::UpdateInternalInputDevicesEventBlocker() {
  // Internal input devices should be blocked (as long as the current
  // tablet_mode_behavior_ allows it) if we're in UI tablet mode, or if the
  // device is in physical tablet state.
  // Note that |is_in_tablet_physical_state_| takes into account whether the
  // device is in docked mode (with no active internal display), in which case
  // internal input devices should NOT be blocked, since the user may still want
  // to use the internal keyboard and mouse in docked mode. This can happen if
  // the user turns off the internal display without closing the lid by means of
  // setting the brightness to 0.
  const bool should_block_internal_events =
      tablet_mode_behavior_.block_internal_input_device &&
      (display::Screen::GetScreen()->InTabletMode() ||
       is_in_tablet_physical_state_);

  if (should_block_internal_events == AreInternalInputDeviceEventsBlocked()) {
    if (force_notify_events_blocking_changed_) {
      for (auto& observer : tablet_mode_observers_) {
        observer.OnTabletModeEventsBlockingChanged();
      }
      force_notify_events_blocking_changed_ = false;
    }

    return;
  }

  event_blocker_->UpdateInternalInputDevices(should_block_internal_events);
  for (auto& observer : tablet_mode_observers_) {
    observer.OnTabletModeEventsBlockingChanged();
  }
}

void TabletModeController::SuspendOcclusionTracker() {
  occlusion_tracker_reset_timer_.Stop();
  occlusion_tracker_pauser_ =
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
  occlusion_tracker_reset_timer_.Start(FROM_HERE, kOcclusionTrackerTimeout,
                                       this,
                                       &TabletModeController::ResetPauser);
}

void TabletModeController::ResetPauser() {
  occlusion_tracker_pauser_.reset();
}

void TabletModeController::FinishInitTabletMode() {
  DCHECK_EQ(display::TabletState::kEnteringTabletMode,
            display::Screen::GetScreen()->GetTabletState());

  // Transition shelf to tablet mode state, now that the screenshot for tablet
  // mode transition was taken. Taking screenshot recreates shelf container
  // layer, and uses the original layer - changing shelf state before the
  // screenshot is taken would change the shelf appearance, and could cause
  // issues where the original shelf widget layer is not re-painted correctly in
  // response to a paint schedule for tablet mode state change.
  // Update the shelf state befire initiating tablet mode window state changes
  // to avoid negative impact of window work-area changes (due to changes in
  // shelf bounds) during window state transition on the animation smoothness
  // https://crbug.com/1044316.
  ShelfConfig::Get()->UpdateForTabletMode(/*in_tablet_mode=*/true);

  tablet_mode_window_manager_ = std::make_unique<TabletModeWindowManager>();
  tablet_mode_window_manager_->Init();

  base::RecordAction(base::UserMetricsAction("Touchview_Enabled"));
  RecordTabletModeUsageInterval(TABLET_MODE_INTERVAL_INACTIVE);
  Shell::Get()->display_manager()->SetTabletState(
      display::TabletState::kInTabletMode);

  // In some cases, TabletModeWindowManager::TabletModeWindowManager uses
  // split view to represent windows that were snapped in desktop mode. If
  // there is a window snapped on one side but no window snapped on the other
  // side, then overview mode should be started (to be seen on the side with
  // no snapped window).
  const auto state =
      SplitViewController::Get(Shell::GetPrimaryRootWindow())->state();
  if (state == SplitViewController::State::kPrimarySnapped ||
      state == SplitViewController::State::kSecondarySnapped) {
    Shell::Get()->overview_controller()->StartOverview(
        OverviewStartAction::kSplitView);
  }

  UpdateInternalInputDevicesEventBlocker();
  Shell::Get()->cursor_manager()->HideCursor();

  VLOG(1) << "Enter tablet mode.";
}

void TabletModeController::DeleteScreenshot() {
  if (screenshot_layer_) {
    VLOG(1) << "Tablet screenshot layer destroyed.";
  }

  screenshot_layer_.reset();
  screenshot_taken_callback_.Cancel();
  screenshot_set_callback_.Cancel();
  ResetDestroyObserver();
  container_hider_.reset();
}

void TabletModeController::ResetDestroyObserver() {
  destroy_observer_.reset();
}

void TabletModeController::TakeScreenshot(aura::Window* top_window) {
  DCHECK(!top_window->IsRootWindow());
  destroy_observer_ = std::make_unique<DestroyObserver>(
      top_window, base::BindOnce(&TabletModeController::ResetDestroyObserver,
                                 weak_factory_.GetWeakPtr()));
  screenshot_set_callback_.Reset(base::BindOnce(
      &TabletModeController::FinishInitTabletMode, weak_factory_.GetWeakPtr()));

  auto* screenshot_window = top_window->GetRootWindow()->GetChildById(
      kShellWindowId_ScreenAnimationContainer);
  base::OnceClosure callback = screenshot_set_callback_.callback();

  aura::Window* root_window = top_window->GetRootWindow();
  container_hider_ = std::make_unique<ScopedContainerHider>(root_window);

  // Request a screenshot.
  screenshot_taken_callback_.Reset(base::BindOnce(
      &TabletModeController::OnLayerCopyed, weak_factory_.GetWeakPtr(),
      std::move(callback), root_window));

  CopyLayerContentToNewLayer(screenshot_window->layer(),
                             screenshot_taken_callback_.callback());

  VLOG(1) << "Tablet screenshot requested.";
}

void TabletModeController::OnLayerCopyed(
    base::OnceClosure on_screenshot_taken,
    aura::Window* root_window,
    std::unique_ptr<ui::Layer> copy_layer) {
  aura::Window* top_window =
      destroy_observer_ ? destroy_observer_->window() : nullptr;
  ResetDestroyObserver();

  container_hider_.reset();

  // Cancel if the root window is deleted while taking a screenshot.
  if (!base::Contains(Shell::GetAllRootWindows(), root_window)) {
    return;
  }

  if (!copy_layer || !top_window) {
    std::move(on_screenshot_taken).Run();
    return;
  }

  // Stack the screenshot under |top_window|, to fully occlude all windows
  // except |top_window| for the duration of the enter tablet mode animation.
  screenshot_layer_ = std::move(copy_layer);
  top_window->parent()->layer()->Add(screenshot_layer_.get());
  screenshot_layer_->SetBounds(top_window->GetRootWindow()->bounds());
  top_window->parent()->layer()->StackBelow(screenshot_layer_.get(),
                                            top_window->layer());

  std::move(on_screenshot_taken).Run();

  VLOG(1) << "Tablet screenshot layer created.";
}

bool TabletModeController::CalculateIsInTabletPhysicalState() const {
  switch (tablet_mode_behavior_.force_physical_tablet_state) {
    case ForcePhysicalTabletState::kDefault:
      // Don't return forced result. Check the hardware configuration.
      break;
    case ForcePhysicalTabletState::kForceTabletMode:
      return true;
    case ForcePhysicalTabletState::kForceClamshellMode:
      return false;
  }

  if (!HasActiveInternalDisplay()) {
    return false;
  }

  // For updated EC, the tablet mode switch activates at 200 degrees, and
  // deactivates at 160 degrees.
  // For old EC, the tablet mode switch activates at 300 degrees, so it's
  // always reliable when |tablet_mode_switch_is_on_|.
  if (tablet_mode_switch_is_on_) {
    return true;
  }

  if (lid_is_closed_) {
    return false;
  }

  if (!can_detect_lid_angle_) {
    return false;
  }

  // Toggle tablet mode on or off when corresponding thresholds are passed.
  if (lid_angle_ >= kEnterTabletModeAngle &&
      (lid_angle_is_stable_ || CanUseUnstableLidAngle())) {
    return true;
  }

  if (lid_angle_ <= kExitTabletModeAngle && lid_angle_is_stable_) {
    // For angles that are in the exit range, we only consider the stable ones,
    // (i.e. we don't check `CanUseUnstableLidAngle()`) in order to avoid
    // changing the mode when the lid is almost closed, or recently opened.
    return false;
  }

  // The state should remain the same.
  return is_in_tablet_physical_state_;
}

bool TabletModeController::ShouldUiBeInTabletMode() const {
  if (forced_ui_mode_ == UiMode::kTabletMode) {
    return true;
  }

  if (forced_ui_mode_ == UiMode::kClamshell) {
    return false;
  }

  if (!tablet_mode_behavior_.observe_pointer_device_events) {
    return is_in_tablet_physical_state_;
  }

  // If this is a tablet capable device, and `OnDeviceListsComplete()` has
  // not been received yet, then skip further checking and don't enter tablet
  // mode, since `has_external_pointing_device_` and
  // `has_internal_pointing_device_` are not accurate yet.
  if (IsBoardTypeMarkedAsTabletCapable() &&
      !initial_input_device_set_up_finished_) {
    return false;
  }

  if (has_external_pointing_device_) {
    return false;
  }

  if (is_in_tablet_physical_state_) {
    return true;
  }

  return !has_internal_pointing_device_ && CanEnterTabletMode() &&
         HasActiveInternalDisplay() && base::SysInfo::IsRunningOnChromeOS();
}

bool TabletModeController::SetIsInTabletPhysicalState(bool new_state) {
  if (new_state == is_in_tablet_physical_state_) {
    return false;
  }

  is_in_tablet_physical_state_ = new_state;

  for (auto& observer : tablet_mode_observers_) {
    observer.OnTabletPhysicalStateChanged();
  }

  // InputDeviceBlocker must always be updated, but don't update it here if the
  // UI state has changed because it's already done.
  if (UpdateUiTabletState()) {
    return true;
  }

  UpdateInternalInputDevicesEventBlocker();
  return false;
}

bool TabletModeController::UpdateUiTabletState() {
  const bool should_be_in_tablet_mode = ShouldUiBeInTabletMode();
  if (should_be_in_tablet_mode ==
      display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  SetTabletModeEnabledInternal(should_be_in_tablet_mode);
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringUTF8(
          should_be_in_tablet_mode ? IDS_ASH_SWITCH_TO_TABLET_MODE
                                   : IDS_ASH_SWITCH_TO_LAPTOP_MODE));
  return true;
}

void TabletModeController::StartTrackingTabletUsageMetricsIfApplicable() {
  if (!CanEnterTabletMode() || !initial_input_device_set_up_finished_ ||
      !have_seen_tablet_mode_event_ ||
      !tablet_mode_usage_interval_start_time_.is_null()) {
    return;
  }

  tablet_mode_usage_interval_start_time_ = base::Time::Now();
}

}  // namespace ash
