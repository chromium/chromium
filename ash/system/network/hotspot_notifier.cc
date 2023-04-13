// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/hotspot_notifier.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
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

const char HotspotNotifier::kHotspotTurnedOnNotificationId[] =
    "cros_hotspot_notifier_ids.hotspot_turned_on";

const char kNotifierHotspot[] = "ash.hotspot";

HotspotNotifier::HotspotNotifier() {
  GetHotspotConfigService(
      remote_cros_hotspot_config_.BindNewPipeAndPassReceiver());
  remote_cros_hotspot_config_->ObserveEnabledStateChanges(
      hotspot_enabled_state_observer_receiver_.BindNewPipeAndPassRemote());

  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  remote_cros_hotspot_config_->AddObserver(
      hotspot_config_observer_receiver_.BindNewPipeAndPassRemote());
}

HotspotNotifier::~HotspotNotifier() = default;

void HotspotNotifier::OnHotspotTurnedOn(bool wifi_turned_off) {
  if (wifi_turned_off) {
    scoped_refptr<message_center::NotificationDelegate> delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(&HotspotNotifier::EnableWiFiHandler,
                                weak_ptr_factory_.GetWeakPtr(),
                                kWiFiTurnedOffNotificationId));

    std::vector<message_center::ButtonInfo> notification_actions;
    std::unique_ptr<message_center::Notification> notification =
        CreateNotification(
            l10n_util::GetStringUTF16(IDS_ASH_HOTSPOT_ON_TITLE),
            l10n_util::GetStringUTF16(IDS_ASH_HOTSPOT_WIFI_TURNED_OFF_MESSAGE),
            kWiFiTurnedOffNotificationId, delegate);
    notification_actions.push_back(
        message_center::ButtonInfo(l10n_util::GetStringUTF16(
            IDS_ASH_HOTSPOT_NOTIFICATION_WIFI_TURN_ON_BUTTON)));
    notification->set_buttons(notification_actions);
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
      CreateNotification(l10n_util::GetStringUTF16(title_id),
                         l10n_util::GetStringUTF16(message_id), notification_id,
                         delegate);

  if (notification_actions.size() > 0) {
    notification->set_buttons(notification_actions);
  }

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification_id,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

void HotspotNotifier::OnHotspotInfoChanged() {
  remote_cros_hotspot_config_->GetHotspotInfo(base::BindOnce(
      &HotspotNotifier::OnGetHotspotInfo, weak_ptr_factory_.GetWeakPtr()));
}

void HotspotNotifier::OnGetHotspotInfo(
    hotspot_config::mojom::HotspotInfoPtr hotspot_info) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (hotspot_info->state == hotspot_config::mojom::HotspotState::kDisabled) {
    message_center->RemoveNotification(kHotspotTurnedOnNotificationId,
                                       /*by_user=*/false);
    return;
  }

  if (hotspot_info->state == hotspot_config::mojom::HotspotState::kEnabled) {
    const std::u16string& title =
        l10n_util::GetStringUTF16(IDS_ASH_HOTSPOT_ON_TITLE);
    const std::u16string& message =
        (hotspot_info->client_count == 0)
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_HOTSPOT_ON_MESSAGE_NO_CONNECTED_DEVICES)
        : (hotspot_info->client_count == 1)
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_HOTSPOT_ON_MESSAGE_ONE_CONNECTED_DEVICE)
            : l10n_util::GetStringFUTF16(
                  IDS_ASH_HOTSPOT_ON_MESSAGE_MULTIPLE_CONNECTED_DEVICES,
                  base::NumberToString16(hotspot_info->client_count));
    scoped_refptr<message_center::NotificationDelegate> delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating(&HotspotNotifier::DisableHotspotHandler,
                                weak_ptr_factory_.GetWeakPtr(),
                                kHotspotTurnedOnNotificationId));
    std::vector<message_center::ButtonInfo> notification_actions;
    notification_actions.push_back(message_center::ButtonInfo(
        l10n_util::GetStringUTF16(IDS_ASH_TURN_OFF_HOTSPOT_LABEL)));
    std::unique_ptr<message_center::Notification> notification =
        CreateNotification(title, message, kHotspotTurnedOnNotificationId,
                           delegate);
    notification->set_pinned(/*pinned=*/true);
    notification->set_buttons(notification_actions);
    message_center->AddNotification(std::move(notification));
  }
}

void HotspotNotifier::DisableHotspotHandler(const char* notification_id,
                                            absl::optional<int> button_index) {
  if (!button_index) {
    return;
  }

  if (button_index.value() == 0) {
    remote_cros_hotspot_config_->DisableHotspot(
        base::BindOnce([](hotspot_config::mojom::HotspotControlResult result) {
          if (result == hotspot_config::mojom::HotspotControlResult::kSuccess ||
              result == hotspot_config::mojom::HotspotControlResult::
                            kAlreadyFulfilled) {
            message_center::MessageCenter* message_center =
                message_center::MessageCenter::Get();
            message_center->RemoveNotification(kHotspotTurnedOnNotificationId,
                                               /*by_user=*/false);
          }
        }));
  }
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

void HotspotNotifier::EnableWiFiHandler(const char* notification_id,
                                        absl::optional<int> button_index) {
  if (!button_index) {
    return;
  }

  if (button_index.value() == 0) {
    remote_cros_network_config_->SetNetworkTypeEnabledState(
        chromeos::network_config::mojom::NetworkType::kWiFi, /*enabled=*/true,
        base::DoNothing());
  }
}

void HotspotNotifier::OnDeviceStateListChanged() {
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &HotspotNotifier::OnGetDeviceStateList, weak_ptr_factory_.GetWeakPtr()));
}

void HotspotNotifier::OnGetDeviceStateList(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
        devices) {
  for (auto& device : devices) {
    if (device->type == chromeos::network_config::mojom::NetworkType::kWiFi &&
        device->device_state ==
            chromeos::network_config::mojom::DeviceStateType::kEnabled) {
      message_center::MessageCenter* message_center =
          message_center::MessageCenter::Get();
      message_center->RemoveNotification(kWiFiTurnedOffNotificationId,
                                         /*by_user=*/false);
      return;
    }
  }
}

std::unique_ptr<message_center::Notification>
HotspotNotifier::CreateNotification(
    const std::u16string& title_id,
    const std::u16string& message_id,
    const char* notification_id,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title_id,
          message_id,
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