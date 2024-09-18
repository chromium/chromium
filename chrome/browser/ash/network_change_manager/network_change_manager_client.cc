// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/network_change_manager/network_change_manager_client.h"

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "net/base/network_change_notifier_passive.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace ash {

namespace {
NetworkChangeManagerClient* g_network_change_manager_client = nullptr;
}

NetworkChangeManagerClient::NetworkChangeManagerClient(
    net::NetworkChangeNotifierPassive* network_change_notifier)
    : connection_type_(net::NetworkChangeNotifier::GetConnectionType()),
      connection_subtype_(net::NetworkChangeNotifier::GetConnectionSubtype()),
      network_change_notifier_(network_change_notifier) {
  DCHECK(!g_network_change_manager_client);
  g_network_change_manager_client = this;

  chromeos::PowerManagerClient::Get()->AddObserver(this);

  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());

  if (content::IsOutOfProcessNetworkService())
    ConnectToNetworkChangeManager();

  // Update initial connection state.
  DefaultNetworkChanged(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
}

NetworkChangeManagerClient::~NetworkChangeManagerClient() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  DCHECK_EQ(g_network_change_manager_client, this);
  g_network_change_manager_client = nullptr;
}

// static
NetworkChangeManagerClient* NetworkChangeManagerClient::GetInstance() {
  return g_network_change_manager_client;
}

void NetworkChangeManagerClient::SuspendDone(base::TimeDelta sleep_duration) {
  // Set `ip_address_changed` to true to force invalidation of network resources
  // on resume.
  NotifyObservers(
      /*dns_changed=*/false, /*ip_address_changed=*/true,
      /*connection_type_changed=*/false,
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN,
      /*connection_subtype_changed=*/false,
      net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_NONE);
}

void NetworkChangeManagerClient::DefaultNetworkChanged(
    const NetworkState* default_network) {
  bool connection_type_changed = false;
  bool connection_subtype_changed = false;
  bool ip_address_changed = false;
  bool dns_changed = false;

  UpdateState(default_network, &dns_changed, &ip_address_changed,
              &connection_type_changed, &connection_subtype_changed);

  NotifyObservers(dns_changed, ip_address_changed, connection_type_changed,
                  connection_type_, connection_subtype_changed,
                  connection_subtype_);
}

void NetworkChangeManagerClient::ConnectToNetworkChangeManager() {
  if (network_change_manager_.is_bound())
    network_change_manager_.reset();

  content::GetNetworkService()->GetNetworkChangeManager(
      network_change_manager_.BindNewPipeAndPassReceiver());
  network_change_manager_.set_disconnect_handler(base::BindOnce(
      &NetworkChangeManagerClient::ReconnectToNetworkChangeManager,
      base::Unretained(this)));
}

void NetworkChangeManagerClient::ReconnectToNetworkChangeManager() {
  ConnectToNetworkChangeManager();

  // Tell the restarted network service what the current connection type is.
  network_change_manager_->OnNetworkChanged(
      /*dns_changed=*/false, /*ip_address_changed=*/false,
      /*connection_type_changed=*/true,
      network::mojom::ConnectionType(connection_type_),
      /*connection_subtype_changed=*/true,
      network::mojom::ConnectionSubtype(connection_subtype_));
}

void NetworkChangeManagerClient::UpdateState(
    const NetworkState* default_network,
    bool* dns_changed,
    bool* ip_address_changed,
    bool* connection_type_changed,
    bool* connection_subtype_changed) {
  *connection_type_changed = false;
  *connection_subtype_changed = false;
  *ip_address_changed = false;
  *dns_changed = false;

  if (!default_network || !default_network->IsConnectedState()) {
    // If we lost a default network, we must update our state and notify
    // observers, otherwise we have nothing to do.
    if (connection_type_ != net::NetworkChangeNotifier::CONNECTION_NONE) {
      NET_LOG(EVENT) << "NCN DefaultNetwork lost"
                     << NetworkPathId(service_path_);
      *ip_address_changed = true;
      *dns_changed = true;
      *connection_type_changed = true;
      *connection_subtype_changed = true;
      connection_type_ = net::NetworkChangeNotifier::CONNECTION_NONE;
      connection_subtype_ = net::NetworkChangeNotifier::SUBTYPE_NONE;
      service_path_.clear();
      ip_address_.clear();
      dns_servers_.clear();
    }
    return;
  }

  // We do have a default network and it is connected.
  net::NetworkChangeNotifier::ConnectionType new_connection_type =
      ConnectionTypeFromShill(default_network->type(),
                              default_network->network_technology());
  if (new_connection_type != connection_type_) {
    NET_LOG(EVENT) << "NCN Default connection type changed: "
                   << net::NetworkChangeNotifier::ConnectionTypeToString(
                          connection_type_)
                   << " -> "
                   << net::NetworkChangeNotifier::ConnectionTypeToString(
                          new_connection_type);
    *connection_type_changed = true;
  }
  if (default_network->path() != service_path_) {
    NET_LOG(EVENT) << "NCN Default network changed: "
                   << NetworkPathId(service_path_) << " -> "
                   << NetworkId(default_network);

    // If we had a default network service change, network resources
    // must always be invalidated.
    *ip_address_changed = true;
    *dns_changed = true;
  }

  std::string new_ip_address = default_network->GetIpAddress();
  if (new_ip_address != ip_address_) {
    // Is this a state update with an online->online transition?
    bool stayed_online =
        (!*connection_type_changed &&
         connection_type_ != net::NetworkChangeNotifier::CONNECTION_NONE);

    bool is_suppressed = true;
    // Suppress IP address change signalling on online->online transitions
    // when getting an IP address update for the first time.
    if (!(stayed_online && ip_address_.empty())) {
      is_suppressed = false;
      *ip_address_changed = true;
    }
    NET_LOG(EVENT) << "NCN Default IPAddress changed"
                   << (is_suppressed ? " (Suppressed)" : "") << ip_address_
                   << " -> " << new_ip_address;
  }
  std::string new_dns_servers = default_network->GetDnsServersAsString();
  if (new_dns_servers != dns_servers_) {
    NET_LOG(EVENT) << "NCN Default DNS server changed" << dns_servers_ << " -> "
                   << new_dns_servers;
    *dns_changed = true;
  }

  connection_type_ = new_connection_type;
  service_path_ = default_network->path();
  ip_address_ = new_ip_address;
  dns_servers_ = new_dns_servers;
  net::NetworkChangeNotifier::ConnectionSubtype new_subtype =
      GetConnectionSubtype(default_network->type(),
                           default_network->network_technology());
  if (new_subtype != connection_subtype_) {
    connection_subtype_ = new_subtype;
    *connection_subtype_changed = true;
  }
}

void NetworkChangeManagerClient::NotifyObservers(
    bool dns_changed,
    bool ip_address_changed,
    bool connection_type_changed,
    net::NetworkChangeNotifier::ConnectionType connection_type,
    bool connection_subtype_changed,
    net::NetworkChangeNotifier::ConnectionSubtype connection_subtype) {
  // If `test_notifications_only_` is set, skip notifying.
  if (network_change_notifier_->IsTestNotificationsOnly())
    return;

  // Notify NetworkChangeNotifier.
  if (ip_address_changed)
    network_change_notifier_->OnIPAddressChanged();
  if (dns_changed)
    network_change_notifier_->OnDNSChanged();
  if (connection_type_changed)
    network_change_notifier_->OnConnectionChanged(connection_type);
  if (connection_subtype_changed || connection_type_changed)
    network_change_notifier_->OnConnectionSubtypeChanged(connection_type,
                                                         connection_subtype);

  // Notify NetworkChangeManager if exists.
  if (network_change_manager_) {
    network_change_manager_->OnNetworkChanged(
        dns_changed, ip_address_changed, connection_type_changed,
        network::mojom::ConnectionType(connection_type),
        connection_subtype_changed,
        network::mojom::ConnectionSubtype(connection_subtype));
  }
}

// static
net::NetworkChangeNotifier::ConnectionType
NetworkChangeManagerClient::ConnectionTypeFromShill(
    const std::string& type,
    const std::string& technology) {
  if (NetworkTypePattern::Ethernet().MatchesType(type))
    return net::NetworkChangeNotifier::CONNECTION_ETHERNET;
  if (type == shill::kTypeWifi)
    return net::NetworkChangeNotifier::CONNECTION_WIFI;

  if (type != shill::kTypeCellular)
    return net::NetworkChangeNotifier::CONNECTION_UNKNOWN;

  // For cellular types, mapping depends on the technology.
  if (technology == shill::kNetworkTechnologyEvdo ||
      technology == shill::kNetworkTechnologyGsm ||
      technology == shill::kNetworkTechnologyUmts ||
      technology == shill::kNetworkTechnologyHspa) {
    return net::NetworkChangeNotifier::CONNECTION_3G;
  }
  if (technology == shill::kNetworkTechnologyHspaPlus ||
      technology == shill::kNetworkTechnologyLte ||
      technology == shill::kNetworkTechnologyLteAdvanced) {
    return net::NetworkChangeNotifier::CONNECTION_4G;
  }
  if (technology == shill::kNetworkTechnology5gNr)
    return net::NetworkChangeNotifier::CONNECTION_5G;

  // Default cellular type is 2G.
  return net::NetworkChangeNotifier::CONNECTION_2G;
}

// static
net::NetworkChangeNotifier::ConnectionSubtype
NetworkChangeManagerClient::GetConnectionSubtype(
    const std::string& type,
    const std::string& technology) {
  if (type != shill::kTypeCellular)
    return net::NetworkChangeNotifier::SUBTYPE_UNKNOWN;

  if (technology == shill::kNetworkTechnology1Xrtt)
    return net::NetworkChangeNotifier::SUBTYPE_1XRTT;
  if (technology == shill::kNetworkTechnologyEvdo)
    return net::NetworkChangeNotifier::SUBTYPE_EVDO_REV_0;
  if (technology == shill::kNetworkTechnologyGsm)
    return net::NetworkChangeNotifier::SUBTYPE_GSM;
  if (technology == shill::kNetworkTechnologyGprs)
    return net::NetworkChangeNotifier::SUBTYPE_GPRS;
  if (technology == shill::kNetworkTechnologyEdge)
    return net::NetworkChangeNotifier::SUBTYPE_EDGE;
  if (technology == shill::kNetworkTechnologyUmts)
    return net::NetworkChangeNotifier::SUBTYPE_UMTS;
  if (technology == shill::kNetworkTechnologyHspa)
    return net::NetworkChangeNotifier::SUBTYPE_HSPA;
  if (technology == shill::kNetworkTechnologyHspaPlus)
    return net::NetworkChangeNotifier::SUBTYPE_HSPAP;
  if (technology == shill::kNetworkTechnologyLte)
    return net::NetworkChangeNotifier::SUBTYPE_LTE;
  if (technology == shill::kNetworkTechnologyLteAdvanced)
    return net::NetworkChangeNotifier::SUBTYPE_LTE_ADVANCED;

  return net::NetworkChangeNotifier::SUBTYPE_UNKNOWN;
}

}  // namespace ash
