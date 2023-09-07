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
         notification_state ==
             PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_OUT;
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
    return gfx::kNoneIcon;
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
    return message_center::SystemNotificationWarningLevel::NORMAL;
  }
}

std::u16string GetLowBatteryTitle(
    PowerNotificationController::NotificationState notification_state) {
  const bool critical_battery =
      notification_state == PowerNotificationController::NOTIFICATION_CRITICAL;

  const bool auto_enable_bsm_notification =
      notification_state ==
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_OUT;

  if (critical_battery) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_CRITICAL_BATTERY_TITLE);
  } else if (features::IsBatterySaverAvailable() &&
             auto_enable_bsm_notification) {
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_TITLE);
  }

  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_TITLE);
}

std::u16string GetLowBatteryMessage(
    PowerNotificationController::NotificationState notification_state,
    const std::u16string& duration,
    double battery_percentage) {
  const bool auto_enable_bsm_notification =
      notification_state ==
      PowerNotificationController::NOTIFICATION_BSM_THRESHOLD_OPT_OUT;

  if (features::IsBatterySaverAvailable() && auto_enable_bsm_notification) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_MESSAGE_WITHOUT_TIME,
        base::NumberToString16(battery_percentage));
  }

  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_LOW_BATTERY_MESSAGE,
                                    duration,
                                    base::NumberToString16(battery_percentage));
}

absl::optional<int> CalculateNotificationButtonToken(
    const PowerStatus& status,
    PowerNotificationController::NotificationState notification_state) {
  const bool no_notification =
      notification_state == PowerNotificationController::NOTIFICATION_NONE;
  const bool critical_battery_notification =
      notification_state == PowerNotificationController::NOTIFICATION_CRITICAL;

  // There are no buttons to add if either battery saver mode isn't available,
  // or if it is available, but there are no notifications showing, or if our
  // battery is critical.
  if (!features::IsBatterySaverAvailable() || no_notification ||
      critical_battery_notification) {
    return absl::nullopt;
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
  absl::optional<int> enable_disable_bsm_token_optional =
      CalculateNotificationButtonToken(status, notification_state);
  if (enable_disable_bsm_token_optional == absl::nullopt) {
    return;
  }

  message_center::ButtonInfo bsm_button{
      l10n_util::GetStringUTF16(enable_disable_bsm_token_optional.value())};
  rich_notification_data.buttons =
      std::vector<message_center::ButtonInfo>{bsm_button};
  rich_notification_data.settings_button_handler =
      message_center::SettingsButtonHandler::DELEGATE;
}

void HandlePowerNotificationButtonClick(
    absl::optional<int> token,
    const absl::optional<int> button_index) {
  if (token == absl::nullopt || button_index == absl::nullopt) {
    return;
  }

  const bool active =
      token.value() == IDS_ASH_STATUS_TRAY_LOW_BATTERY_BSM_BUTTON_OPT_IN;

  // Handle Button functionality based on button pressed, and button text.
  switch (button_index.value()) {
    case 0: {
      Shell::Get()->battery_saver_controller()->SetState(
          active, BatterySaverController::UpdateReason::kThreshold);
      break;
    }
    default:
      NOTREACHED_NORETURN();
  }
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
  } else if (time &&
             (power_utils::ShouldDisplayBatteryTime(*time) ||
              features::IsBatterySaverAvailable()) &&
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
      message = GetLowBatteryMessage(notification_state, duration,
                                     battery_percentage);
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
              CalculateNotificationButtonToken(status, notification_state))),
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
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->RemoveNotification(kNotificationId, false);
  }
}

void BatteryNotification::Update(
    PowerNotificationController::NotificationState notification_state) {
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->UpdateNotification(kNotificationId,
                                        CreateNotification(notification_state));
  }
}

}  // namespace ash
