// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_notification.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/power_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/power_status.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::HandleNotificationClickDelegate;
using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kNotifierBattery[] = "ash.battery";

const gfx::VectorIcon& GetBatteryImageMD(
    PowerNotificationController::NotificationState notification_state) {
  if (PowerStatus::Get()->IsUsbChargerConnected()) {
    return kNotificationBatteryFluctuatingIcon;
  } else if (notification_state ==
             PowerNotificationController::NOTIFICATION_LOW_POWER) {
    return kNotificationBatteryLowIcon;
  } else if (notification_state ==
             PowerNotificationController::NOTIFICATION_CRITICAL) {
    return kNotificationBatteryCriticalIcon;
  } else {
    NOTREACHED();
    return gfx::kNoneIcon;
  }
}

message_center::SystemNotificationWarningLevel GetWarningLevelMD(
    PowerNotificationController::NotificationState notification_state) {
  if (PowerStatus::Get()->IsUsbChargerConnected()) {
    return message_center::SystemNotificationWarningLevel::NORMAL;
  } else if (notification_state ==
             PowerNotificationController::NOTIFICATION_LOW_POWER) {
    return message_center::SystemNotificationWarningLevel::WARNING;
  } else if (notification_state ==
             PowerNotificationController::NOTIFICATION_CRITICAL) {
    return message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else {
    NOTREACHED();
    return message_center::SystemNotificationWarningLevel::NORMAL;
  }
}

std::u16string GetLowBatteryTitle(
    const PowerStatus& status,
    PowerNotificationController::NotificationState notification_state) {
  const bool critical_battery =
      notification_state == PowerNotificationController::NOTIFICATION_CRITICAL;
  const bool low_battery =
      notification_state == PowerNotificationController::NOTIFICATION_LOW_POWER;
  const bool bsm_active = status.IsBatterySaverActive();

  if (critical_battery) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_CRITICAL_BATTERY_TITLE);
  } else if (low_battery && bsm_active) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_TITLE);
  }

  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE);
}

std::u16string GetLowBatteryMessage(
    const PowerStatus& status,
    PowerNotificationController::NotificationState notification_state,
    const std::u16string& duration,
    double battery_percentage) {
  const bool bsm_active = status.IsBatterySaverActive();
  const bool low_power_notification =
      notification_state ==
      PowerNotificationController::NotificationState::NOTIFICATION_LOW_POWER;

  auto message = low_power_notification && bsm_active
                     ? IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_MESSAGE
                     : IDS_ASH_STATUS_TRAY_LOW_BATTERY_MESSAGE;

  return l10n_util::GetStringFUTF16(message, duration,
                                    base::NumberToString16(battery_percentage));
}

void HandleDisableBatterySaverModeClick(
    const absl::optional<int> button_index) {
  power_manager::SetBatterySaverModeStateRequest bsm_request;
  bsm_request.set_enabled(false);
  chromeos::PowerManagerClient::Get()->SetBatterySaverModeState(bsm_request);
}

std::unique_ptr<Notification> CreateNotification(
    PowerNotificationController::NotificationState notification_state) {
  const PowerStatus& status = *PowerStatus::Get();

  const double battery_percentage = status.GetRoundedBatteryPercent();

  std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_TITLE);
  std::u16string message = base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_PERCENT),
      battery_percentage / 100.0);

  const absl::optional<base::TimeDelta> time =
      status.IsBatteryCharging() ? status.GetBatteryTimeToFull()
                                 : status.GetBatteryTimeToEmpty();

  message_center::RichNotificationData rich_notification_data;

  if (status.IsUsbChargerConnected()) {
    title =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_TITLE);
    message = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
  } else if (time && power_utils::ShouldDisplayBatteryTime(*time) &&
             !status.IsBatteryDischargingOnLinePower()) {
    std::u16string duration = ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG, *time);
    if (status.IsBatteryCharging()) {
      title =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_TITLE);
      message = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL, duration);
    } else {
      // This is a low battery warning prompting the user in minutes.
      title = GetLowBatteryTitle(status, notification_state);
      message = GetLowBatteryMessage(status, notification_state, duration,
                                     battery_percentage);

      // Low battery notifications should display on fullscreen windows.
      rich_notification_data.fullscreen_visibility =
          message_center::FullscreenVisibility::OVER_USER;

      // Display the 'turn off battery saver' button only on the low battery
      // notification.
      if (notification_state == PowerNotificationController::NotificationState::
                                    NOTIFICATION_LOW_POWER &&
          status.IsBatterySaverActive()) {
        message_center::ButtonInfo bsm_button{l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON)};
        rich_notification_data.buttons =
            std::vector<message_center::ButtonInfo>{bsm_button};
        rich_notification_data.settings_button_handler =
            message_center::SettingsButtonHandler::DELEGATE;
      }
    }
  }

  std::unique_ptr<Notification> notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      BatteryNotification::kNotificationId, title, message, std::u16string(),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierBattery,
                                 NotificationCatalogName::kBatteryNotifier),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleDisableBatterySaverModeClick)),
      GetBatteryImageMD(notification_state),
      GetWarningLevelMD(notification_state));
  if (notification_state ==
      PowerNotificationController::NOTIFICATION_CRITICAL) {
    notification->SetSystemPriority();
    notification->set_pinned(true);
  }
  return notification;
}

}  // namespace

// static
const char BatteryNotification::kNotificationId[] = "battery";

BatteryNotification::BatteryNotification(
    MessageCenter* message_center,
    PowerNotificationController::NotificationState notification_state)
    : message_center_(message_center) {
  message_center_->AddNotification(CreateNotification(notification_state));
}

BatteryNotification::~BatteryNotification() {
  if (message_center_->FindVisibleNotificationById(kNotificationId))
    message_center_->RemoveNotification(kNotificationId, false);
}

void BatteryNotification::Update(
    PowerNotificationController::NotificationState notification_state) {
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->UpdateNotification(kNotificationId,
                                        CreateNotification(notification_state));
  }
}

}  // namespace ash
