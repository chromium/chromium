// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_POWER_POWER_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_status.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

class BatteryNotification;
class DualRoleNotification;

// Controller class to manage power/battery notifications.
class ASH_EXPORT PowerNotificationController
    : public PowerStatus::Observer,
      public chromeos::PowerManagerClient::Observer,
      public ash::ShellObserver {
 public:
  enum NotificationState {
    NOTIFICATION_NONE,

    // Low battery charge, different battery saver notification behavior.
    // Note: When battery saver is not available, both of these act like the
    // original low power notification.
    NOTIFICATION_BSM_ENABLING_AT_THRESHOLD,

    NOTIFICATION_BSM_THRESHOLD_OPT_IN,

    NOTIFICATION_GENERIC_LOW_POWER,

    // Critically low battery charge.
    NOTIFICATION_CRITICAL,
  };

  enum CriticalNotificationOutcome {
    // The device crashes, it includes the case when the battery is empty and
    // powerd does not have time to perform a graceful shutdown.
    Crashed = 1,
    // The device automatically shut down due to a low battery.
    LowBatteryShutdown = 2,
    // The critical notification is shown, its count should be greater than or
    // equal to the sum of all other outcomes.
    NotificationShown = 0,
    // The device is connected to a power source.
    PluggedIn = 3,
    // The device enters a suspended state.
    Suspended = 4,
    // The device is shut down gracefully by user.
    UserShutdown = 5,
    kMaxValue = UserShutdown,
  };

  // Time-based notification thresholds when on battery power.
  static constexpr int kCriticalMinutes = 5;
  static constexpr int kLowPowerMinutes = 15;
  static constexpr int kNoWarningMinutes = 30;

  // Percentage-based notification thresholds when using a low-power charger.
  static constexpr int kCriticalPercentage = 5;
  static constexpr int kLowPowerPercentage = 10;
  static constexpr int kNoWarningPercentage = 15;

  explicit PowerNotificationController(
      message_center::MessageCenter* message_center);

  PowerNotificationController(const PowerNotificationController&) = delete;
  PowerNotificationController& operator=(const PowerNotificationController&) =
      delete;

  ~PowerNotificationController() override;

  // static:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void NotifyUsbNotificationClosedByUser();
  void SetUserOptStatus(bool status);

  double GetLowPowerPercentage() const { return low_power_percentage_; }
  double GetCriticalPowerPercentage() const { return critical_percentage_; }
  double GetNoWarningPercentage() const { return no_warning_percentage_; }

  NotificationState GetNotificationState() const { return notification_state_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(PowerNotificationControllerTest,
                           MaybeShowUsbChargerNotification);
  FRIEND_TEST_ALL_PREFIXES(PowerNotificationControllerTest,
                           UpdateNotificationState);
  friend class PowerNotificationControllerTest;
  friend class BatteryNotificationTest;

  // Overridden from PowerStatus::Observer.
  void OnPowerStatusChanged() override;

  // Overridden from PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void ShutdownRequested(power_manager::RequestShutdownReason reason) override;
  void RestartRequested(power_manager::RequestRestartReason reason) override;

  // Overridden from ash::ShellObserver:
  void OnShellDestroying() override;

  // Shows a notification that a low-power USB charger has been connected.
  // Returns true if a notification was shown or explicitly hidden.
  bool MaybeShowUsbChargerNotification();

  // Shows a notification when dual-role devices are connected.
  void MaybeShowDualRoleNotification();

  // Records the outcome of a critical notification.
  void MaybeRecordCriticalNotificationOutcome(
      PowerNotificationController::CriticalNotificationOutcome outcome,
      base::TimeDelta duration);

  // Determines whether a Battery Saver Notification should be shown. Returns
  // true if a notification should be shown, or nullopt if none of the bsm
  // branches were triggered.
  std::optional<bool> HandleBatterySaverNotifications();

  // Sets |notification_state_|. Returns true if a notification should be shown.
  bool UpdateNotificationState();
  bool UpdateNotificationStateForRemainingTime();
  bool UpdateNotificationStateForRemainingPercentage();
  bool UpdateNotificationStateForRemainingPercentageBatterySaver();

  // Whether the device is plugged in during a critical battery state.
  bool PluggedInCriticalState();

  // Start a timer and update the kCriticalStateDuration every 15 seconds.
  void StartPeriodicUpdate();
  void UpdateCriticalNotificationDurationPrefs();

  // Reset the timestamp related to critical notification.
  void ResetCriticalNotificationTimestamp();

  static const char kUsbNotificationId[];

  raw_ptr<PrefService> local_state_;                             // Unowned.
  const raw_ptr<message_center::MessageCenter> message_center_;  // Unowned.
  std::unique_ptr<BatteryNotification> battery_notification_;
  std::unique_ptr<DualRoleNotification> dual_role_notification_;
  NotificationState notification_state_ = NOTIFICATION_NONE;

  // Was the battery full the last time OnPowerStatusChanged() was called?
  bool battery_was_full_ = false;

  // Was a USB charger connected the last time OnPowerStatusChanged() was
  // called?
  bool usb_charger_was_connected_ = false;

  // Was line power connected the last time onPowerStatusChanged() was called?
  bool line_power_was_connected_ = false;

  // Was the battery in critical state the last time onPowerStatusChanged() was
  // called?
  bool was_in_critical_state_ = false;

  // The remaining battery time the last time OnPowerStatusChanged() was called.
  // This value is utilized to determine the remaining battery time at the
  // moment the charger is connected.
  std::optional<base::TimeDelta> remaining_time_to_empty_from_critical_state_;

  // Has the user already dismissed a low-power notification? Should be set
  // back to false when all power sources are disconnected.
  bool usb_notification_dismissed_ = false;

  // Has the battery saver threshold been crossed? Also gets reset to false when
  // an AC charger is plugged in.
  bool battery_saver_triggered_ = false;

  // User opt status.
  bool user_opt_status_ = false;

  const double battery_saver_activation_charge_percent_;

  // Percentage-based notification thresholds for battery saver.
  // TODO(mwoj): Replace the static constexpr once data is collected from the
  // experiment.
  const int critical_percentage_;
  const int low_power_percentage_;
  const int no_warning_percentage_;

  // After critical notification shows, trigger
  // `UpdateCriticalNotificationDurationPrefs` periodically.
  base::RepeatingTimer timer_;

  // The time at which a critical notification is shown.
  base::TimeTicks critical_notification_shown_time_ = base::TimeTicks();

  // The observation on `ash::Shell`.
  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_NOTIFICATION_CONTROLLER_H_
