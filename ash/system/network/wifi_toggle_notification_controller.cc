// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/wifi_toggle_notification_controller.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::NetworkType;
using message_center::Notification;

namespace ash {

namespace {

constexpr char kWifiToggleNotificationId[] = "wifi-toggle";
constexpr char kNotifierWifiToggle[] = "ash.wifi-toggle";

std::unique_ptr<Notification> CreateNotification(bool wifi_enabled) {
  const int string_id = wifi_enabled
                            ? IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED
                            : IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED;
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kWifiToggleNotificationId,
      std::u16string(), l10n_util::GetStringUTF16(string_id),
      network_icon::GetImageModelForWiFiEnabledState(wifi_enabled),
      /*display_source=*/std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierWifiToggle,
                                 NotificationCatalogName::kWifiToggle),
      message_center::RichNotificationData(), nullptr);
  return notification;
}

}  // namespace

WifiToggleNotificationController::WifiToggleNotificationController() {
  Shell::Get()->system_tray_notifier()->AddNetworkObserver(this);
}

WifiToggleNotificationController::~WifiToggleNotificationController() {
  Shell::Get()->system_tray_notifier()->RemoveNetworkObserver(this);
}

void WifiToggleNotificationController::RequestToggleWifi() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  // Remove any existing notification.
  if (message_center->FindVisibleNotificationById(kWifiToggleNotificationId))
    message_center->RemoveNotification(kWifiToggleNotificationId, false);

  TrayNetworkStateModel* model =
      Shell::Get()->system_tray_model()->network_state_model();
  const DeviceStateProperties* wifi = model->GetDevice(NetworkType::kWiFi);
  // A WiFi device should always exist, but the model is not part of Shell
  // so just return to handle the edge case.
  if (!wifi)
    return;
  bool enabled = wifi->device_state == DeviceStateType::kEnabled;
  base::RecordAction(
      enabled ? base::UserMetricsAction("StatusArea_Network_WifiDisabled")
              : base::UserMetricsAction("StatusArea_Network_WifiEnabled"));
  model->SetNetworkTypeEnabledState(NetworkType::kWiFi, !enabled);

  // Create a new notification with the new state.
  message_center->AddNotification(CreateNotification(!enabled));
}

}  // namespace ash
