// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_HOTSPOT_NOTIFIER_H_
#define ASH_SYSTEM_NETWORK_HOTSPOT_NOTIFIER_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
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
    : public hotspot_config::mojom::HotspotEnabledStateObserver {
 public:
  HotspotNotifier();
  HotspotNotifier(const HotspotNotifier&) = delete;
  HotspotNotifier& operator=(const HotspotNotifier&) = delete;
  ~HotspotNotifier() override;

  static const char kWiFiTurnedOffNotificationId[];
  static const char kAdminRestrictedNotificationId[];

 private:
  friend class HotspotNotifierTest;

  // HotspotEnabledStateObserver:
  void OnHotspotTurnedOn(bool wifi_turned_off) override;
  void OnHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason) override;

  std::unique_ptr<message_center::Notification> CreateNotification(
      const int title_id,
      const int message_id,
      const char* notification_id,
      scoped_refptr<message_center::NotificationDelegate> delegate);

  mojo::Remote<hotspot_config::mojom::CrosHotspotConfig>
      remote_cros_hotspot_config_;
  mojo::Receiver<hotspot_config::mojom::HotspotEnabledStateObserver>
      hotspot_enabled_state_observer_receiver_{this};

  base::WeakPtrFactory<HotspotNotifier> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_HOTSPOT_NOTIFIER_H_