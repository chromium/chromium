// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_NOTIFIER_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_NOTIFIER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/message_center/message_center.h"

namespace ash {

// Notifies the user about following hotspot events:
//  - WiFi has been turned off to enable hotspot
//  - Hotspot has been disabled due to following reasons -
//  -  1. Internal error
//  -  2. Admin policy
//  -  3. WiFi has been enabled
//  -  4. In activity
//  - Hotspot is turned on and has 'n' active connections
class ASH_EXPORT HotspotNotifier
    : public hotspot_config::mojom::HotspotEnabledStateObserver,
      public hotspot_config::mojom::CrosHotspotConfigObserver,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  HotspotNotifier();
  HotspotNotifier(const HotspotNotifier&) = delete;
  HotspotNotifier& operator=(const HotspotNotifier&) = delete;
  ~HotspotNotifier() override;

  static const char kAdminRestrictedNotificationId[];
  static const char kWiFiTurnedOnNotificationId[];
  static const char kAutoDisabledNotificationId[];
  static const char kInternalErrorNotificationId[];
  static const char kHotspotTurnedOnNotificationId[];

 private:
  friend class HotspotNotifierTest;

  // HotspotEnabledStateObserver:
  void OnHotspotTurnedOn() override;
  void OnHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason) override;

  // CrosNetworkConfigObserver:
  void OnDeviceStateListChanged() override;

  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  // mojom::CrosHotspotConfigObserver:
  void OnHotspotInfoChanged() override;

  void OnGetHotspotInfo(hotspot_config::mojom::HotspotInfoPtr hotspot_info);

  void DisableHotspotHandler(const char* notification_id,
                             std::optional<int> index);

  std::unique_ptr<message_center::Notification> CreateNotification(
      const std::u16string& title_id,
      const std::u16string& message_id,
      const char* notification_id,
      const bool use_hotspot_icon,
      scoped_refptr<message_center::NotificationDelegate> delegate);

  void EnableHotspotHandler(const char* notification_id,
                            std::optional<int> index);
  void EnableWiFiHandler(const char* notification_id, std::optional<int> index);

  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig>
      remote_cros_hotspot_config_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};
  mojo::Receiver<hotspot_config::mojom::HotspotEnabledStateObserver>
      hotspot_enabled_state_observer_receiver_{this};
  mojo::Receiver<hotspot_config::mojom::CrosHotspotConfigObserver>
      hotspot_config_observer_receiver_{this};

  hotspot_config::mojom::HotspotAllowStatus allow_status_ =
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoMobileData;

  base::WeakPtrFactory<HotspotNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_NOTIFIER_H_
