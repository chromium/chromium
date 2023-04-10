// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/hotspot_notifier.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {

// static
const char HotspotNotifier::kWiFiTurnedOffNotificationId[] =
    "cros_hotspot_notifier_ids.wifi_turned_off";

const char HotspotNotifier::kAdminRestrictedNotificationId[] =
    "cros_hotspot_notifier_ids.admin_restricted";

const char HotspotNotifier::kWiFiTurnedOnNotificationId[] =
    "cros_hotspot_notifier_ids.wifi_turned_on";

const char HotspotNotifier::kAutoDisabledNotificationId[] =
    "cros_hotspot_notifier_ids.auto_disabled";

const char HotspotNotifier::kInternalErrorNotificationId[] =
    "cros_hotspot_notifier_ids.internal_error";

const char kNotifierHotspot[] = "ash.hotspot";

HotspotNotifier::HotspotNotifier() {
  GetHotspotConfigService(
      remote_cros_hotspot_config_.BindNewPipeAndPassReceiver());
  remote_cros_hotspot_config_->ObserveEnabledStateChanges(
      hotspot_enabled_state_observer_receiver_.BindNewPipeAndPassRemote());
}

HotspotNotifier::~HotspotNotifier() = default;

void HotspotNotifier::OnHotspotTurnedOn(bool wifi_turned_off) {
  if (wifi_turned_off) {
    std::unique_ptr<message_center::Notification> notification =
        CreateNotification(IDS_ASH_HOTSPOT_ON_TITLE,
                           IDS_ASH_HOTSPOT_WIFI_TURNED_OFF_MESSAGE,
                           kWiFiTurnedOffNotificationId,
                           /*delegate=*/nullptr);

    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    message_center->RemoveNotification(kWiFiTurnedOffNotificationId,
                                       /*by_user=*/false);
    message_center->AddNotification(std::move(notification));
  }
}

void HotspotNotifier::OnHotspotTurnedOff(
    hotspot_config::mojom::DisableReason disable_reason) {
  scoped_refptr<message_center::NotificationDelegate> delegate = nullptr;
  int title_id;
  int message_id;
  const char* notification_id;
  std::vector<message_center::ButtonInfo> notification_actions;
  switch (disable_reason) {
    case hotspot_config::mojom::DisableReason::kProhibitedByPolicy:
      title_id = IDS_ASH_HOTSPOT_OFF_TITLE;
      message_id = IDS_ASH_HOTSPOT_ADMIN_RESTRICTED_MESSAGE;
      notification_id = kAdminRestrictedNotificationId;
      break;
    case hotspot_config::mojom::DisableReason::kWifiEnabled:
      title_id = IDS_ASH_HOTSPOT_OFF_TITLE;
      message_id = IDS_ASH_HOTSPOT_WIFI_TURNED_ON_MESSAGE;
      notification_id = kWiFiTurnedOnNotificationId;
      break;
    case hotspot_config::mojom::DisableReason::kAutoDisabled:
      title_id = IDS_ASH_HOTSPOT_OFF_TITLE;
      message_id = IDS_ASH_HOTSPOT_AUTO_DISABLED_MESSAGE;
      notification_id = kAutoDisabledNotificationId;
      delegate =
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&HotspotNotifier::EnableHotspotHandler,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  notification_id));
      notification_actions.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_ASH_HOTSPOT_NOTIFICATION_TURN_ON_BUTTON)));
      break;
    case hotspot_config::mojom::DisableReason::kInternalError:
      title_id = IDS_ASH_HOTSPOT_OFF_TITLE;
      message_id = IDS_ASH_HOTSPOT_INTERNAL_ERROR_MESSAGE;
      notification_id = kInternalErrorNotificationId;
      delegate =
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&HotspotNotifier::EnableHotspotHandler,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  notification_id));
      notification_actions.push_back(
          message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_ASH_HOTSPOT_NOTIFICATION_TURN_ON_BUTTON)));
      break;
    default:
      return;
  }
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification(title_id, message_id, notification_id, delegate);

  if (notification_actions.size() > 0) {
    notification->set_buttons(notification_actions);
  }

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification_id,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

void HotspotNotifier::EnableHotspotHandler(const char* notification_id,
                                           absl::optional<int> button_index) {
  if (!button_index) {
    return;
  }

  if (button_index.value() == 0) {
    remote_cros_hotspot_config_->EnableHotspot(
        base::BindOnce([](hotspot_config::mojom::HotspotControlResult result) {
          if (result == hotspot_config::mojom::HotspotControlResult::kSuccess ||
              result == hotspot_config::mojom::HotspotControlResult::
                            kAlreadyFulfilled) {
            message_center::MessageCenter* message_center =
                message_center::MessageCenter::Get();
            message_center->RemoveNotification(kAutoDisabledNotificationId,
                                               /*by_user=*/false);
            message_center->RemoveNotification(kInternalErrorNotificationId,
                                               /*by_user=*/false);
          }
        }));
  }
}

std::unique_ptr<message_center::Notification>
HotspotNotifier::CreateNotification(
    const int title_id,
    const int message_id,
    const char* notification_id,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          l10n_util::GetStringUTF16(title_id),
          l10n_util::GetStringUTF16(message_id),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotifierHotspot,
              NotificationCatalogName::kHotspot),
          message_center::RichNotificationData(), delegate,
          /*small_image=*/gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  return notification;
}

}  // namespace ash