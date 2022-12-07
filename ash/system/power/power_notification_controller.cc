// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_notification_controller.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_notification.h"
#include "ash/system/power/dual_role_notification.h"
#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
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

  PowerNotificationController* const controller_;
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
}

PowerNotificationController::~PowerNotificationController() {
  PowerStatus::Get()->RemoveObserver(this);
  message_center_->RemoveNotification(kUsbNotificationId, false);
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
        message_center_, notification_state_);
  } else if (notification_state_ == NOTIFICATION_NONE) {
    battery_notification_.reset();
  } else if (battery_notification_.get()) {
    battery_notification_->Update(notification_state_);
  }

  battery_was_full_ = PowerStatus::Get()->IsBatteryFull();
  usb_charger_was_connected_ = PowerStatus::Get()->IsUsbChargerConnected();
  line_power_was_connected_ = PowerStatus::Get()->IsLinePowerConnected();
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

bool PowerNotificationController::UpdateNotificationState() {
  const PowerStatus& status = *PowerStatus::Get();
  if (!status.IsBatteryPresent() || status.IsBatteryTimeBeingCalculated() ||
      status.IsMainsChargerConnected()) {
    notification_state_ = NOTIFICATION_NONE;
    return false;
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
