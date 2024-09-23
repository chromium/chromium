// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_H_

#include <memory>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/system/power/power_button_controller_base.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/wm/lock_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/views/widget/widget.h"

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class LockStateController;
class PowerButtonDisplayController;
class PowerButtonScreenshotController;

// Handles power button and lock button events. Holding the power button
// displays a menu and later shuts down on all devices. Tapping the power button
// of convertible/slate/detachable devices (except forced clamshell set by
// command line) will turn screen off but nothing will happen for clamshell
// devices. In tablet mode, power button may also be consumed to take a
// screenshot.
class ASH_EXPORT PowerButtonController
    : public PowerButtonControllerBase,
      public display::DisplayObserver,
      public display::DisplayConfigurator::Observer,
      public chromeos::PowerManagerClient::Observer,
      public AccelerometerReader::Observer,
      public ScreenBacklightObserver,
      public LockStateObserver,
      public SessionObserver {
 public:
  enum class ButtonType {
    // Indicates normal power button type.
    NORMAL,

    // Indicates legacy power button type. It could be set by command-line
    // switch telling us that we're running on hardware that misreports power
    // button releases.
    LEGACY,
  };

  // The physical display side of power button.
  enum class PowerButtonPosition { NONE, LEFT, TOP, RIGHT, BOTTOM };

  // Amount of time since last screen state change that power button event needs
  // to be ignored.
  static constexpr base::TimeDelta kScreenStateChangeDelay =
      base::Milliseconds(500);

  // Ignore button-up events occurring within this many milliseconds of the
  // previous button-up event. This prevents us from falling behind if the power
  // button is pressed repeatedly.
  static constexpr base::TimeDelta kIgnoreRepeatedButtonUpDelay =
      base::Milliseconds(500);

  // Amount of time since last SuspendDone() that power button event needs to be
  // ignored.
  static constexpr base::TimeDelta kIgnorePowerButtonAfterResumeDelay =
      base::Seconds(2);

  // Value of switches::kAshPowerButtonPosition stored in JSON format. These
  // are the field names of the flag.
  static constexpr const char* kEdgeField = "edge";
  static constexpr const char* kPositionField = "position";

  // Value of |kEdgeField|.
  static constexpr const char* kLeftEdge = "left";
  static constexpr const char* kRightEdge = "right";
  static constexpr const char* kTopEdge = "top";
  static constexpr const char* kBottomEdge = "bottom";

  explicit PowerButtonController(
      BacklightsForcedOffSetter* backlights_forced_off_setter);

  PowerButtonController(const PowerButtonController&) = delete;
  PowerButtonController& operator=(const PowerButtonController&) = delete;

  ~PowerButtonController() override;

  // Handles events from "legacy" ACPI power buttons. On devices with these
  // buttons (typically Chromeboxes), button releases are misreported
  // immediately after button presses, regardless of how long the button
  // is actually held.
  void OnLegacyPowerButtonEvent(bool down);

  // Handles events from "normal" power buttons where button presses and
  // releases are both reported accurately.
  void OnPowerButtonEvent(bool down, const base::TimeTicks& timestamp);

  // Handles lock button behavior.
  void OnLockButtonEvent(bool down, const base::TimeTicks& timestamp);

  // True if the menu is opened.
  bool IsMenuOpened() const;

  // Called when DEBUG_TOGGLE_POWER_BUTTON_MENU is pressed. This is used to help
  // bring up the menu for debugging without pressing the physical power
  // button. The menu will be shown without pre-shutdown.
  void ShowMenuOnDebugAccelerator();

  // Dismisses the menu.
  void DismissMenu();

  // Do not force backlights to be turned off.
  void StopForcingBacklightsOff();

  // PowerButtonControllerBase:
  void OnArcPowerButtonMenuEvent() override;
  void CancelPowerButtonEvent() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // display::DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;

  // chromeos::PowerManagerClient::Observer:
  void ScreenBrightnessChanged(
      const power_manager::BacklightBrightnessChange& change) override;
  void PowerButtonEventReceived(bool down, base::TimeTicks timestamp) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // SessionObserver:
  void OnLoginStatusChanged(LoginStatus status) override;

  // Initializes |screenshot_controller_| according to the tablet mode switch in
  // |result|.
  void OnGetSwitchStates(
      std::optional<chromeos::PowerManagerClient::SwitchStates> result);

  // TODO(minch): Remove this if/when all applicable devices expose a tablet
  // mode switch: https://crbug.com/798646.
  // AccelerometerReader::Observer:
  void OnECLidAngleDriverStatusChanged(bool is_supported) override {}
  void OnAccelerometerUpdated(const AccelerometerUpdate& update) override;

  // BacklightsForcedOffSetter::Observer:
  void OnBacklightsForcedOffChanged(bool forced_off) override;
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

  // Used by the `ash::curtain::Session` to notify when power button is
  // enabled/disabled.
  void OnSecurityCurtainEnabled();
  void OnSecurityCurtainDisabled();

  // LockStateObserver:
  void OnLockStateEvent(LockStateObserver::EventType event) override;

 private:
  friend class PowerButtonControllerTestApi;

  // Returns true if tablet power button behavior (i.e. tapping the button turns
  // the screen off) should currently be used.
  bool UseTabletBehavior() const;

  // Stops |power_button_menu_timer_|, |shutdown_timer_| and dismisses the power
  // button menu.
  void StopTimersAndDismissMenu();

  // Starts the power menu animation. Called when a clamshell device's power
  // button is pressed, or when |power_button_menu_timer_| fires, or by arc
  // power button to show the PowerButtonMenu.
  void StartPowerMenuAnimation(ShutdownReason reason);

  // Called by |pre_shutdown_timer_| to start the cancellable pre-shutdown
  // animation.
  void OnPreShutdownTimeout();

  // Updates |button_type_| and power button position info based on the current
  // command line.
  void ProcessCommandLine();

  // Initializes tablet power button behavior related member
  // |screenshot_controller_|.
  void InitTabletPowerButtonMembers();

  // Locks the screen if the "Show lock screen when waking from sleep" pref is
  // set and locking is possible.
  void LockScreenIfRequired();

  // Sets |show_menu_animation_done_| to true.
  void SetShowMenuAnimationDone();

  // A helper function called by ProcessCommandLine to parse the value of
  // switches::kAshPowerButtonPosition.
  void ParsePowerButtonPositionSwitch();

  // Updates UMA histogram of power button press according to the power button
  // up state. |up_state| is a bit field containing values from the
  // PowerButtonUpState enum defined in the .cc file.
  void UpdatePowerButtonEventUMAHistogram(uint32_t up_state);

  // Are the power or lock buttons currently held?
  bool power_button_down_ = false;
  bool lock_button_down_ = false;

  // Passed in StartPowerMenuAnimation(ShutdownReason reason). When it is not
  // POWER_BUTTON such as when called from arc, we do not start
  // |pre_shutdown_timer_|.
  ShutdownReason shutdown_reason_ = ShutdownReason::POWER_BUTTON;

  // True if the device is currently in tablet mode (per TabletModeController).
  bool in_tablet_mode_ = false;

  // Has the screen brightness been reduced to 0%?
  bool brightness_is_zero_ = false;

  // True if an internal display is off while an external display is on (e.g.
  // for Chrome OS's docked mode, where a Chromebook's lid is closed while an
  // external display is connected).
  bool internal_display_off_and_external_display_on_ = false;

  // True after the animation that shows the power menu has finished.
  bool show_menu_animation_done_ = false;

  // Saves the button type for this power button.
  ButtonType button_type_ = ButtonType::NORMAL;

  // True if the kForceTabletPowerButton flag is set. This forces tablet power
  // button behavior even while in laptop mode.
  bool force_tablet_power_button_ = false;

  // True if the screen was off when the power button was pressed.
  bool screen_off_when_power_button_down_ = false;

  // True if power menu is already shown when pressing the power button.
  bool menu_shown_when_power_button_down_ = false;

  // True if the next button release event should force the display off.
  bool force_off_on_button_up_ = false;

  // Used to force backlights off, when needed.
  raw_ptr<BacklightsForcedOffSetter>
      backlights_forced_off_setter_;  // Not owned.

  raw_ptr<LockStateController> lock_state_controller_;  // Not owned.

  // Time source for performed action times.
  raw_ptr<const base::TickClock> tick_clock_;

  // Used to interact with the display.
  std::unique_ptr<PowerButtonDisplayController> display_controller_;

  // Handles events for power button screenshot.
  std::unique_ptr<PowerButtonScreenshotController> screenshot_controller_;

  // Saves the most recent timestamp that powerd resumed from suspend,
  // updated in SuspendDone().
  base::TimeTicks last_resume_time_;

  // Saves the most recent timestamp that power button was released.
  base::TimeTicks last_button_up_time_;

  // Started when |show_menu_animation_done_| is set to true and stopped when
  // power button is released. Runs OnPreShutdownTimeout() to start the
  // cancellable pre-shutdown animation.
  base::OneShotTimer pre_shutdown_timer_;

  // Started when the power button of convertible/slate/detachable devices is
  // pressed and stopped when it's released. Runs StartPowerMenuAnimation() to
  // show the power button menu.
  base::OneShotTimer power_button_menu_timer_;

  // The fullscreen widget of power button menu.
  std::unique_ptr<views::Widget> menu_widget_;

  // The physical display side of power button in landscape primary.
  PowerButtonPosition power_button_position_ = PowerButtonPosition::NONE;

  // The center of the power button's offset from the top of the screen (for
  // left/right) or left side of the screen (for top/bottom) in
  // landscape_primary. Values are in [0.0, 1.0] and express a fraction of the
  // display's height or width, respectively.
  double power_button_offset_percentage_ = 0.f;

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_{this};

  // Used to maintain active state of the active window that exists before
  // showing menu.
  std::unique_ptr<views::Widget::PaintAsActiveLock>
      active_window_paint_as_active_lock_;

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<PowerButtonController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_CONTROLLER_H_
