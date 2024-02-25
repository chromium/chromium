// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_notification_controller.h"

#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

constexpr char kNotifierId[] = "adaptive-charging-notify";
constexpr char kInfoNotificationId[] = "adaptive-charging-notify-info";
constexpr base::TimeDelta kTimeDeltaRoundingInterval = base::Minutes(30);

}  // namespace

AdaptiveChargingNotificationController::
    AdaptiveChargingNotificationController() = default;

AdaptiveChargingNotificationController::
    ~AdaptiveChargingNotificationController() = default;

void AdaptiveChargingNotificationController::ShowAdaptiveChargingNotification(
    std::optional<base::TimeDelta> time_to_full) {
  if (!ShouldShowNotification())
    return;

  std::u16string notification_message;
  if (time_to_full.has_value()) {
    DCHECK(time_to_full.value().is_positive());
    notification_message = l10n_util::GetStringFUTF16(
        IDS_ASH_ADAPTIVE_CHARGING_NOTIFICATION_MESSAGE_TEMPORARY,
        base::TimeFormatTimeOfDayWithHourClockType(
            base::Time::FromDeltaSinceWindowsEpoch(
                (base::Time::Now().ToDeltaSinceWindowsEpoch() +
                 time_to_full.value())
                    .RoundToMultiple(kTimeDeltaRoundingInterval)),
            base::GetHourClockType(), base::kKeepAmPm));
  } else {
    notification_message = l10n_util::GetStringUTF16(
        IDS_ASH_ADAPTIVE_CHARGING_NOTIFICATION_MESSAGE_INDEFINITE);
  }

  message_center::RichNotificationData notification_data;
  notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_ADAPTIVE_CHARGING_NOTIFICATION_FULLY_CHARGE_NOW_BUTTON_TEXT)));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kInfoNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_ADAPTIVE_CHARGING_NOTIFICATION_TITLE),
      notification_message,
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kAdaptiveCharging),
      notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()),
      kAdaptiveChargingBatteryIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_accent_color_id(cros_tokens::kCrosSysPositive);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void AdaptiveChargingNotificationController::CloseAdaptiveChargingNotification(
    bool by_user) {
  message_center::MessageCenter::Get()->RemoveNotification(kInfoNotificationId,
                                                           by_user);
}

bool AdaptiveChargingNotificationController::ShouldShowNotification() {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();

  return pref_service &&
         pref_service->GetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled);
}

void AdaptiveChargingNotificationController::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index.has_value())
    return;
  if (button_index.value() == 0) {
    chromeos::PowerManagerClient::Get()->ChargeNowForAdaptiveCharging();
    CloseAdaptiveChargingNotification(/*by_user=*/true);
  } else {
    NOTREACHED() << "Unknown button index value";
  }
}

}  // namespace ash
