// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_notification_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/dual_role_notification.h"
#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {
namespace {

const char kNotifierPower[] = "ash.power";

// Informs the PowerNotificationController when a USB notification is closed.
class UsbNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit UsbNotificationDelegate(PowerNotificationController* controller)
      : controller_(controller) {}

  UsbNotificationDelegate(const UsbNotificationDelegate&) = delete;
  UsbNotificationDelegate& operator=(const UsbNotificationDelegate&) = delete;

  // Overridden from message_center::NotificationDelegate.
  void Close(bool by_user) override {
    if (by_user)
      controller_->NotifyUsbNotificationClosedByUser();
  }

 private:
  ~UsbNotificationDelegate() override = default;

  const raw_ptr<PowerNotificationController, ExperimentalAsh> controller_;
};

std::string GetNotificationStateString(
    PowerNotificationController::NotificationState notification_state) {
  switch (notification_state) {
    case PowerNotificationController::NOTIFICATION_NONE:
      return "none";
    case PowerNotificationController::NOTIFICATION_LOW_POWER:
      return "low power";
    case PowerNotificationController::NOTIFICATION_CRITICAL:
      return "critical power";
    case PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_OUT:
      return "20% remaining - battery saver opt out";
    case PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN:
      return "20% remaining - battery saver opt in";
    case PowerNotificationController::NOTIFICATION_BSM_LOW_POWER_OPT_IN:
      return "15 min remaining - battery saver opt in";
  }
  NOTREACHED() << "Unknown state " << notification_state;
  return "Unknown state";
}

void LogBatteryForUsbCharger(
    PowerNotificationController::NotificationState state,
    int battery_percent) {
  VLOG(1) << "Showing " << GetNotificationStateString(state)
          << " notification. USB charger is connected. "
          << "Battery percentage: " << battery_percent << "%.";
}

void LogBatteryForNoCharger(
    PowerNotificationController::NotificationState state,
    int remaining_minutes) {
  VLOG(1) << "Showing " << GetNotificationStateString(state)
          << " notification. No charger connected."
          << " Remaining time: " << remaining_minutes << " minutes.";
}

}  // namespace

const char PowerNotificationController::kUsbNotificationId[] = "usb-charger";

PowerNotificationController::PowerNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  PowerStatus::Get()->AddObserver(this);
  battery_saver_previously_active_ = PowerStatus::Get()->IsBatterySaverActive();
}

PowerNotificationController::~PowerNotificationController() {
  PowerStatus::Get()->RemoveObserver(this);
  message_center_->RemoveNotification(kUsbNotificationId, false);
}

void PowerNotificationController::MaybeResetNotificationAvailability(
    features::BatterySaverNotificationBehavior experiment,
    const double battery_percent,
    const int battery_remaining_minutes) {
  if (battery_remaining_minutes > kLowPowerMinutes) {
    low_power_crossed_ = false;
  }

  if (battery_percent > BatterySaverController::kActivationChargePercent) {
    threshold_crossed_ = false;
  }
}

void PowerNotificationController::OnPowerStatusChanged() {
  bool battery_alert = UpdateNotificationState();

  // Factory testing may place the battery into unusual states.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshHideNotificationsForFactory)) {
    return;
  }

  MaybeShowUsbChargerNotification();
  MaybeShowDualRoleNotification();

  if (battery_alert) {
    // Remove any existing notification so it's dismissed before adding a new
    // one. Otherwise we might update a "low battery" notification to "critical"
    // without it being shown again.
    battery_notification_.reset();
    battery_notification_ = std::make_unique<BatteryNotification>(
        message_center_, notification_state_, battery_saver_previously_active_);
  } else if (notification_state_ == NOTIFICATION_NONE) {
    battery_notification_.reset();
  } else if (battery_notification_.get()) {
    battery_notification_->Update(notification_state_,
                                  battery_saver_previously_active_);
  }

  battery_was_full_ = PowerStatus::Get()->IsBatteryFull();
  usb_charger_was_connected_ = PowerStatus::Get()->IsUsbChargerConnected();
  line_power_was_connected_ = PowerStatus::Get()->IsLinePowerConnected();
  battery_saver_previously_active_ = PowerStatus::Get()->IsBatterySaverActive();
}

bool PowerNotificationController::MaybeShowUsbChargerNotification() {
  const PowerStatus& status = *PowerStatus::Get();

  // We show the notification if a USB charger is connected but the battery
  // isn't full (since some ECs may choose to use a lower power rail when the
  // battery is full even when a high-power charger is connected).
  const bool show = status.IsUsbChargerConnected() && !status.IsBatteryFull();

  // Check if the notification needs to be created.
  if (show && !usb_charger_was_connected_ && !usb_notification_dismissed_) {
    bool on_battery = PowerStatus::Get()->IsBatteryPresent();
    std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
        message_center::NOTIFICATION_TYPE_SIMPLE, kUsbNotificationId,
        l10n_util::GetStringUTF16(
            on_battery ? IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_TITLE
                       : IDS_ASH_STATUS_TRAY_LOW_POWER_ADAPTER_TITLE),
        on_battery
            ? ui::SubstituteChromeOSDeviceType(
                  IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_MESSAGE_SHORT)
            : l10n_util::GetStringFUTF16(
                  IDS_ASH_STATUS_TRAY_LOW_POWER_ADAPTER_MESSAGE_SHORT,
                  ui::GetChromeOSDeviceName(),
                  base::FormatDouble(
                      PowerStatus::Get()->GetPreferredMinimumPower(), 0)),
        std::u16string(), GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kNotifierPower,
            on_battery ? NotificationCatalogName::kLowPowerCharger
                       : NotificationCatalogName::kLowPowerAdapter),
        message_center::RichNotificationData(),
        new UsbNotificationDelegate(this), kNotificationLowPowerChargerIcon,
        message_center::SystemNotificationWarningLevel::WARNING);
    notification->set_pinned(on_battery);
    notification->set_never_timeout(!on_battery);
    message_center_->AddNotification(std::move(notification));
    return true;
  }

  if (!show && usb_charger_was_connected_ && !battery_was_full_) {
    // USB charger was unplugged or identified as a different type or battery
    // reached the full state while the notification was showing.
    message_center_->RemoveNotification(kUsbNotificationId, false);
    if (!status.IsLinePowerConnected())
      usb_notification_dismissed_ = false;
    return true;
  }

  return false;
}

void PowerNotificationController::MaybeShowDualRoleNotification() {
  const PowerStatus& status = *PowerStatus::Get();
  if (!status.HasDualRoleDevices()) {
    dual_role_notification_.reset();
    return;
  }

  if (!dual_role_notification_)
    dual_role_notification_ =
        std::make_unique<DualRoleNotification>(message_center_);
  dual_role_notification_->Update();
}

absl::optional<bool>
PowerNotificationController::HandleBatterySaverNotifications() {
  const PowerStatus& status = *PowerStatus::Get();

  const absl::optional<base::TimeDelta> remaining_time =
      status.GetBatteryTimeToEmpty();

  // Check that powerd actually provided an estimate. It doesn't if the battery
  // current is so close to zero that the estimate would be huge.
  if (!remaining_time) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  const bool bsm_currently_active = status.IsBatterySaverActive();
  const double tte = *remaining_time / base::Minutes(1);
  const int remaining_minutes = base::ClampRound(tte);
  const int remaining_percentage = status.GetRoundedBatteryPercent();

  const bool is_20_percent_or_lower_notification =
      remaining_percentage <= BatterySaverController::kActivationChargePercent;

  const bool low_power_minutes_notification =
      remaining_minutes <= PowerNotificationController::kLowPowerMinutes &&
      remaining_minutes > PowerNotificationController::kCriticalMinutes;

  const bool no_notification_currently_showing =
      notification_state_ == NOTIFICATION_NONE;

  const features::BatterySaverNotificationBehavior experiment =
      features::kBatterySaverNotificationBehavior.Get();

  // Notification State Machine based on experiment arms for battery saver.
  switch (experiment) {
    case features::kFullyAutoEnable:
      // Initial Opt-Out Notification at 20% battery.
      if (is_20_percent_or_lower_notification &&
          !battery_saver_previously_active_ && bsm_currently_active &&
          no_notification_currently_showing && !threshold_crossed_) {
        notification_state_ =
            PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_OUT;
        threshold_crossed_ = true;
        return true;
      }

      // Secondary Opt-Out Low-Power Notification at 15 minutes remaining.
      if (low_power_minutes_notification && !battery_saver_previously_active_ &&
          bsm_currently_active && !low_power_crossed_) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        low_power_crossed_ = true;
        return false;
      }
      break;
    case features::kOptInThenAutoEnable:
      // Initial Opt-In Notification at 20% battery.
      if (is_20_percent_or_lower_notification &&
          !battery_saver_previously_active_ && !bsm_currently_active &&
          !threshold_crossed_) {
        notification_state_ = NOTIFICATION_BSM_THRESHOLD_OPT_IN;
        threshold_crossed_ = true;
        return true;
      }

      // Secondary Opt-Out Low-Power Notification at 15 minutes remaining.
      // If we haven't crossed the threshold, then let the low power
      // notification handle it.
      if (low_power_minutes_notification && low_power_crossed_) {
        return false;
      }
      if (low_power_minutes_notification && !low_power_crossed_) {
        low_power_crossed_ = true;
      }
      break;
    case features::kFullyOptIn:
      // Initial Opt-In Notification at 20% battery.
      if (is_20_percent_or_lower_notification &&
          !battery_saver_previously_active_ && !bsm_currently_active &&
          !threshold_crossed_) {
        notification_state_ =
            PowerNotificationController::NOTIFICATION_BSM_LOW_POWER_OPT_IN;
        threshold_crossed_ = true;
        return true;
      }

      // Secondary Opt-In Low-Power Notification at 15 minutes remaining.
      if (low_power_minutes_notification && battery_saver_previously_active_ &&
          bsm_currently_active && !low_power_crossed_) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        low_power_crossed_ = true;
        return true;
      } else if (low_power_minutes_notification &&
                 !battery_saver_previously_active_ && !bsm_currently_active &&
                 !low_power_crossed_) {
        notification_state_ = NOTIFICATION_BSM_LOW_POWER_OPT_IN;
        low_power_crossed_ = true;
        return true;
      }
      break;
    default:
      break;
  }

  return absl::nullopt;
}

bool PowerNotificationController::UpdateNotificationState() {
  const PowerStatus& status = *PowerStatus::Get();
  const absl::optional<base::TimeDelta> remaining_time =
      status.GetBatteryTimeToEmpty();

  // Reset threshold when charging and percent/minutes remaining go above their
  // respective thresholds.
  if (features::IsBatterySaverAvailable() &&
      (status.IsMainsChargerConnected() || status.IsUsbChargerConnected() ||
       status.IsLinePowerConnected()) &&
      remaining_time) {
    const features::BatterySaverNotificationBehavior experiment =
        features::kBatterySaverNotificationBehavior.Get();
    const double tte = *remaining_time / base::Minutes(1);
    const int remaining_minutes = base::ClampRound(tte);
    const int remaining_percentage = status.GetRoundedBatteryPercent();
    MaybeResetNotificationAvailability(experiment, remaining_percentage,
                                       remaining_minutes);
  }

  if (!status.IsBatteryPresent() || status.IsBatteryTimeBeingCalculated() ||
      status.IsMainsChargerConnected()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  if (features::IsBatterySaverAvailable()) {
    absl::optional<bool> should_update = HandleBatterySaverNotifications();
    if (should_update != absl::nullopt) {
      return should_update.value();
    }
  }

  return status.IsUsbChargerConnected()
             ? UpdateNotificationStateForRemainingPercentage()
             : UpdateNotificationStateForRemainingTime();
}

bool PowerNotificationController::UpdateNotificationStateForRemainingTime() {
  const absl::optional<base::TimeDelta> remaining_time =
      PowerStatus::Get()->GetBatteryTimeToEmpty();

  // Check that powerd actually provided an estimate. It doesn't if the battery
  // current is so close to zero that the estimate would be huge.
  if (!remaining_time) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  // The notification includes a rounded minutes value, so round the estimate
  // received from the power manager to match.
  const int remaining_minutes =
      base::ClampRound(*remaining_time / base::Minutes(1));

  if (remaining_minutes >= kNoWarningMinutes ||
      PowerStatus::Get()->IsBatteryFull()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
    case NOTIFICATION_BSM_THRESHOLD_OPT_OUT:
    case NOTIFICATION_BSM_THRESHOLD_OPT_IN:
      if (remaining_minutes <= kCriticalMinutes) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForNoCharger(notification_state_, remaining_minutes);
        return true;
      }
      if (remaining_minutes <= kLowPowerMinutes) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        LogBatteryForNoCharger(notification_state_, remaining_minutes);
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
    case NOTIFICATION_BSM_LOW_POWER_OPT_IN:
      if (remaining_minutes <= kCriticalMinutes) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForNoCharger(notification_state_, remaining_minutes);
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

bool PowerNotificationController::
    UpdateNotificationStateForRemainingPercentage() {
  // The notification includes a rounded percentage, so round the value received
  // from the power manager to match.
  const int remaining_percentage =
      PowerStatus::Get()->GetRoundedBatteryPercent();

  if (remaining_percentage >= kNoWarningPercentage ||
      PowerStatus::Get()->IsBatteryFull()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
  }

  switch (notification_state_) {
    case NOTIFICATION_NONE:
    case NOTIFICATION_BSM_THRESHOLD_OPT_OUT:
    case NOTIFICATION_BSM_THRESHOLD_OPT_IN:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForUsbCharger(notification_state_, remaining_percentage);
        return true;
      }
      if (remaining_percentage <= kLowPowerPercentage) {
        notification_state_ = NOTIFICATION_LOW_POWER;
        LogBatteryForUsbCharger(notification_state_, remaining_percentage);
        return true;
      }
      return false;
    case NOTIFICATION_LOW_POWER:
    case NOTIFICATION_BSM_LOW_POWER_OPT_IN:
      if (remaining_percentage <= kCriticalPercentage) {
        notification_state_ = NOTIFICATION_CRITICAL;
        LogBatteryForUsbCharger(notification_state_, remaining_percentage);
        return true;
      }
      return false;
    case NOTIFICATION_CRITICAL:
      return false;
  }
  NOTREACHED();
  return false;
}

void PowerNotificationController::NotifyUsbNotificationClosedByUser() {
  usb_notification_dismissed_ = true;
}

}  // namespace ash
