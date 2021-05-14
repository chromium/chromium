// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/battery_notification.h"

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

std::unique_ptr<Notification> CreateNotification(
    PowerNotificationController::NotificationState notification_state) {
  const PowerStatus& status = *PowerStatus::Get();

  std::u16string message = base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_PERCENT),
      static_cast<double>(status.GetRoundedBatteryPercent()) / 100.0);

  const absl::optional<base::TimeDelta> time =
      status.IsBatteryCharging() ? status.GetBatteryTimeToFull()
                                 : status.GetBatteryTimeToEmpty();
  std::u16string time_message;
  if (status.IsUsbChargerConnected()) {
    time_message = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
  } else if (time && power_utils::ShouldDisplayBatteryTime(*time) &&
             !status.IsBatteryDischargingOnLinePower()) {
    if (status.IsBatteryCharging()) {
      std::u16string duration;
      if (!TimeDurationFormat(*time, base::DURATION_WIDTH_NARROW, &duration))
        LOG(ERROR) << "Failed to format duration " << *time;
      time_message = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL, duration);
    } else {
      // This is a low battery warning prompting the user in minutes.
      time_message = ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                            ui::TimeFormat::LENGTH_LONG, *time);
    }
  }

  if (!time_message.empty())
    message = message + u"\n" + time_message;

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      BatteryNotification::kNotificationId, std::u16string(), message,
      std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierBattery),
      message_center::RichNotificationData(), nullptr,
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
