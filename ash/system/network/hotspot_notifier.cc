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
  if (disable_reason ==
      hotspot_config::mojom::DisableReason::kProhibitedByPolicy) {
    std::unique_ptr<message_center::Notification> notification =
        CreateNotification(IDS_ASH_HOTSPOT_OFF_TITLE,
                           IDS_ASH_HOTSPOT_ADMIN_RESTRICTED_MESSAGE,
                           kAdminRestrictedNotificationId,
                           /*delegate=*/nullptr);

    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    message_center->RemoveNotification(kAdminRestrictedNotificationId,
                                       /*by_user=*/false);
    message_center->AddNotification(std::move(notification));
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