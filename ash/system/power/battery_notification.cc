// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_notification.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/power_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_saver_controller.h"
#include "ash/system/power/power_notification_controller.h"
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

bool IsNotificationLowPower(
    PowerNotificationController::NotificationState notification_state) {
  return notification_state ==
             PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN ||
         notification_state == PowerNotificationController::
                                   NOTIFICATION_BSM_ENABLING_AT_THRESHOLD ||
         notification_state ==
             PowerNotificationController::NOTIFICATION_GENERIC_LOW_POWER;
}

const gfx::VectorIcon& GetBatteryImageMD(
    PowerNotificationController::NotificationState notification_state) {
  if (PowerStatus::Get()->IsUsbChargerConnected()) {
    return kNotificationBatteryFluctuatingIcon;
  } else if (IsNotificationLowPower(notification_state)) {
    return kNotificationBatteryLowIcon;
  } else if (notification_state ==
             PowerNotificationController::NOTIFICATION_CRITICAL) {
    return kNotificationBatteryCriticalIcon;
  } else {
    NOTREACHED();
  }
}

message_center::SystemNotificationWarningLevel GetWarningLevelMD(
    PowerNotificationController::NotificationState notification_state) {
  if (PowerStatus::Get()->IsUsbChargerConnected()) {
    return message_center::SystemNotificationWarningLevel::NORMAL;
  } else if (IsNotificationLowPower(notification_state)) {
    return message_center::SystemNotificationWarningLevel::WARNING;
  } else if (notification_state ==
             PowerNotificationController::NOTIFICATION_CRITICAL) {
    return message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  } else {
    NOTREACHED();
  }
}

std::u16string GetLowBatteryTitle(
    PowerNotificationController::NotificationState notification_state) {
  const bool critical_battery =
      notification_state == PowerNotificationController::NOTIFICATION_CRITICAL;

  const bool enabling_at_threshold_notification =
      IsBatterySaverAllowed() &&
      notification_state ==
          PowerNotificationController::NOTIFICATION_BSM_ENABLING_AT_THRESHOLD;

  if (critical_battery) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_CRITICAL_BATTERY_TITLE);
  } else if (enabling_at_threshold_notification) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_AUTOENABLED_TITLE);
  }

  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE);
}

std::u16string GetLowBatteryMessage(
    PowerNotificationController::NotificationState notification_state,
    const std::u16string& duration,
    const double battery_percentage,
    const bool should_display_time) {
  const bool critical_notification =
      notification_state == PowerNotificationController::NOTIFICATION_CRITICAL;

  const bool enabling_at_threshold_notification =
      notification_state ==
      PowerNotificationController::NOTIFICATION_BSM_ENABLING_AT_THRESHOLD;

  const bool opt_in_at_threshold_notification =
      notification_state ==
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN;

  const bool generic_low_power_notification =
      notification_state ==
      PowerNotificationController::NOTIFICATION_GENERIC_LOW_POWER;

  // Send notification immediately with only battery percentage, but update
  // string to battery percentage + time remaining when available.
  if (IsBatterySaverAllowed() && enabling_at_threshold_notification) {
    return should_display_time
               ? l10n_util::GetStringFUTF16(
                     IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_AUTOENABLED_MESSAGE,
                     base::NumberToString16(battery_percentage), duration)
               : l10n_util::GetStringFUTF16(
                     IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_AUTOENABLED_MESSAGE_WITHOUT_TIME,
                     base::NumberToString16(battery_percentage));
  }

  if (IsBatterySaverAllowed() &&
      (opt_in_at_threshold_notification || generic_low_power_notification ||
       critical_notification)) {
    return should_display_time
               ? l10n_util::GetStringFUTF16(
                     IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_GENERIC_MESSAGE,
                     base::NumberToString16(battery_percentage), duration)
               : l10n_util::GetStringFUTF16(
                     IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_GENERIC_MESSAGE_WITHOUT_TIME,
                     base::NumberToString16(battery_percentage));
  }

  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_MESSAGE,
                                    duration,
                                    base::NumberToString16(battery_percentage));
}

std::optional<int> CalculateNotificationButtonToken(
    const PowerStatus& status,
    PowerNotificationController::NotificationState notification_state) {
  const bool no_notification =
      notification_state == PowerNotificationController::NOTIFICATION_NONE;
  const bool generic_low_power_notification =
      notification_state ==
      PowerNotificationController::NOTIFICATION_GENERIC_LOW_POWER;
  const bool critical_battery_notification =
      notification_state == PowerNotificationController::NOTIFICATION_CRITICAL;

  // There are no buttons to add if either battery saver mode isn't available,
  // or if it is available, but there are no notifications showing, or if our
  // battery is a generic low power or critical notification.
  if (!IsBatterySaverAllowed() || no_notification ||
      generic_low_power_notification || critical_battery_notification) {
    return std::nullopt;
  }

  // Note: At this point, the Notification State could be OPT_OUT, or OPT_IN.
  const bool is_notification_opt_in =
      notification_state ==
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_IN;

  return is_notification_opt_in
             ? IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON_OPT_IN
             : IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON_OPT_OUT;
}

void CalculateNotificationButtons(
    const PowerStatus& status,
    PowerNotificationController::NotificationState notification_state,
    message_center::RichNotificationData& rich_notification_data) {
  std::optional<int> enable_disable_bsm_token_optional =
      CalculateNotificationButtonToken(status, notification_state);
  if (enable_disable_bsm_token_optional == std::nullopt) {
    return;
  }

  message_center::ButtonInfo bsm_button{
      l10n_util::GetStringUTF16(enable_disable_bsm_token_optional.value())};
  rich_notification_data.buttons =
      std::vector<message_center::ButtonInfo>{bsm_button};
}

void HandlePowerNotificationButtonClick(
    std::optional<int> token,
    PowerNotificationController* power_notification_controller,
    const std::optional<int> button_index) {
  if (token == std::nullopt || button_index == std::nullopt) {
    return;
  }

  const bool active =
      token.value() == IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON_OPT_IN;

  // Handle Button functionality based on button pressed, and button text.
  switch (button_index.value()) {
    case 0: {
      Shell::Get()->battery_saver_controller()->SetState(
          active, BatterySaverController::UpdateReason::kThreshold);

      if (power_notification_controller) {
        power_notification_controller->SetUserOptStatus(true);
      }

      // Send an 'Enabled' toast if enabling via button.
      if (active) {
        Shell::Get()
            ->battery_saver_controller()
            ->ShowBatterySaverModeEnabledToast();
      }

      // Dismiss notification from Message Center.
      message_center::MessageCenter::Get()->RemoveNotification(
          BatteryNotification::kNotificationId, false);
      break;
    }
    default:
      NOTREACHED();
  }
}

std::unique_ptr<Notification> CreateNotification(
    PowerNotificationController* power_notification_controller) {
  const PowerStatus& status = *PowerStatus::Get();

  const double battery_percentage = status.GetRoundedBatteryPercent();

  const auto notification_state =
      power_notification_controller->GetNotificationState();

  std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_TITLE);
  std::u16string message = base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_PERCENT),
      battery_percentage / 100.0);

  const std::optional<base::TimeDelta> time =
      status.IsBatteryCharging() ? status.GetBatteryTimeToFull()
                                 : status.GetBatteryTimeToEmpty();

  message_center::RichNotificationData rich_notification_data;

  if (status.IsUsbChargerConnected()) {
    title =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_POWER_CHARGER_TITLE);
    message = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
  } else if (time &&
             (power_utils::ShouldDisplayBatteryTime(*time) ||
              IsBatterySaverAllowed()) &&
             !status.IsBatteryDischargingOnLinePower()) {
    std::u16string duration = ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG, *time);
    if (status.IsBatteryCharging()) {
      title =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_TITLE);
      message = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL, duration);
    } else {
      // Low battery notifications should display on fullscreen windows.
      rich_notification_data.fullscreen_visibility =
          message_center::FullscreenVisibility::OVER_USER;

      // Calculate the title, message, and buttons based on the power state.
      title = GetLowBatteryTitle(notification_state);
      message =
          GetLowBatteryMessage(notification_state, duration, battery_percentage,
                               power_utils::ShouldDisplayBatteryTime(*time));
      CalculateNotificationButtons(status, notification_state,
                                   rich_notification_data);
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
          base::BindRepeating(
              &HandlePowerNotificationButtonClick,
              CalculateNotificationButtonToken(status, notification_state),
              power_notification_controller)),
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
    PowerNotificationController* power_notification_controller)
    : message_center_(message_center),
      power_notification_controller_(power_notification_controller) {
  message_center_->AddNotification(
      CreateNotification(power_notification_controller_));
}

BatteryNotification::~BatteryNotification() {
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->RemoveNotification(kNotificationId, false);
  }
}

void BatteryNotification::Update() {
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->UpdateNotification(
        kNotificationId, CreateNotification(power_notification_controller_));
  }
}

}  // namespace ash
