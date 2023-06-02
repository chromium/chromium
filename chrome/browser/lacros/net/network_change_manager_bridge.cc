// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/network_change_manager_bridge.h"

#include "base/functional/bind.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "net/base/network_change_notifier_passive.h"
#include "services/network/public/mojom/network_service.mojom.h"

// Check ConnectionType and ConnectionSubtype values are the same for
// network::mojom and crosapi::mojom.
#define STATIC_ASSERT_CONNECTION_TYPE(v)                 \
  static_assert(static_cast<int>(crosapi::mojom::v) ==   \
                    static_cast<int>(network::mojom::v), \
                "mismatching enums: " #v)
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_UNKNOWN);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_ETHERNET);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_WIFI);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_2G);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_3G);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_4G);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_NONE);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_BLUETOOTH);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_5G);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionType::CONNECTION_LAST);

STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_UNKNOWN);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_NONE);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_OTHER);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_GSM);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_IDEN);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_CDMA);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_1XRTT);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_GPRS);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_EDGE);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_UMTS);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_EVDO_REV_0);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_EVDO_REV_A);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_HSPA);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_EVDO_REV_B);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_HSDPA);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_HSUPA);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_EHRPD);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_HSPAP);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_LTE);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_LTE_ADVANCED);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_BLUETOOTH_1_2);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_BLUETOOTH_2_1);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_BLUETOOTH_3_0);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_BLUETOOTH_4_0);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_ETHERNET);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_FAST_ETHERNET);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_GIGABIT_ETHERNET);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_10_GIGABIT_ETHERNET);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_WIFI_B);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_WIFI_G);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_WIFI_N);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_WIFI_AC);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_WIFI_AD);
STATIC_ASSERT_CONNECTION_TYPE(ConnectionSubtype::SUBTYPE_LAST);

NetworkChangeManagerBridge::NetworkChangeManagerBridge()
    : network_change_notifier_(static_cast<net::NetworkChangeNotifierPassive*>(
          content::GetNetworkChangeNotifier())) {
  auto* lacros_service = chromeos::LacrosService::Get();
  // If NetworkChange crosapi is not supported, fallback to use
  // NetworkChangeNotifierLinux instead.
  if (!lacros_service->IsAvailable<crosapi::mojom::NetworkChange>())
    return;

  if (content::IsOutOfProcessNetworkService())
    ConnectToNetworkChangeManager();

  lacros_service->GetRemote<crosapi::mojom::NetworkChange>()->AddObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

NetworkChangeManagerBridge::~NetworkChangeManagerBridge() = default;

void NetworkChangeManagerBridge::OnNetworkChanged(
    bool dns_changed,
    bool ip_address_changed,
    bool connection_type_changed,
    crosapi::mojom::ConnectionType new_connection_type,
    bool connection_subtype_changed,
    crosapi::mojom::ConnectionSubtype new_connection_subtype) {
  // If `test_notifications_only_` is set for registered notifier, skip
  // notifying.
  if (network_change_notifier_->IsTestNotificationsOnly())
    return;

  DCHECK(network_change_notifier_);
  if (ip_address_changed)
    network_change_notifier_->OnIPAddressChanged();
  if (dns_changed)
    network_change_notifier_->OnDNSChanged();
  if (connection_type_changed)
    network_change_notifier_->OnConnectionChanged(
        net::NetworkChangeNotifier::ConnectionType(new_connection_type));
  if (connection_type_changed || connection_subtype_changed) {
    network_change_notifier_->OnConnectionSubtypeChanged(
        net::NetworkChangeNotifier::ConnectionType(new_connection_type),
        net::NetworkChangeNotifier::ConnectionSubtype(new_connection_subtype));
  }

  if (network_change_manager_) {
    network_change_manager_->OnNetworkChanged(
        dns_changed, ip_address_changed, connection_type_changed,
        network::mojom::ConnectionType(new_connection_type),
        connection_subtype_changed,
        network::mojom::ConnectionSubtype(new_connection_subtype));
  }

  // Update cached values.
  connection_type_ =
      net::NetworkChangeNotifier::ConnectionType(new_connection_type);
  connection_subtype_ =
      net::NetworkChangeNotifier::ConnectionSubtype(new_connection_subtype);
}

void NetworkChangeManagerBridge::ConnectToNetworkChangeManager() {
  if (network_change_manager_.is_bound())
    network_change_manager_.reset();

  content::GetNetworkService()->GetNetworkChangeManager(
      network_change_manager_.BindNewPipeAndPassReceiver());
  network_change_manager_.set_disconnect_handler(base::BindOnce(
      &NetworkChangeManagerBridge::ReconnectToNetworkChangeManager,
      base::Unretained(this)));
}

void NetworkChangeManagerBridge::ReconnectToNetworkChangeManager() {
  ConnectToNetworkChangeManager();

  network_change_manager_->OnNetworkChanged(
      /*dns_changed=*/false, /*ip_address_changed=*/false,
      /*connection_type_changed=*/true,
      network::mojom::ConnectionType(connection_type_),
      /*connection_subtype_changed=*/true,
      network::mojom::ConnectionSubtype(connection_subtype_));
}
