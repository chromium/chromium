// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/discovery/netbios_host_locator.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/smb_client/smb_constants.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_change_notifier.h"

namespace ash::smb_client {
namespace {

bool IsMLan(const net::NetworkInterface& interface) {
  return interface.type == net::NetworkChangeNotifier::CONNECTION_UNKNOWN &&
         base::StartsWith(interface.name, "mlan",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

net::IPAddress CalculateBroadcastAddress(
    const net::NetworkInterface& interface) {
  net::IPAddress internet_address = interface.address;
  const uint32_t inverted_net_mask = 0xffffffff >> interface.prefix_length;

  net::IPAddressBytes bytes = internet_address.bytes();

  uint8_t b0 = bytes[0] | (inverted_net_mask >> 24);
  uint8_t b1 = bytes[1] | (inverted_net_mask >> 16);
  uint8_t b2 = bytes[2] | (inverted_net_mask >> 8);
  uint8_t b3 = bytes[3] | (inverted_net_mask);

  net::IPAddress broadcast_address(b0, b1, b2, b3);
  return broadcast_address;
}

// TODO(baileyberro): Some devices' wifi interface has CONNECTION_UNKNOWN as
// type rather than CONNECTION_WIFI. https://crbug.com/872665
bool ShouldUseInterface(const net::NetworkInterface& interface) {
  return interface.address.IsIPv4() &&
         interface.prefix_length < (net::IPAddress::kIPv4AddressSize * 8) &&
         (interface.type == net::NetworkChangeNotifier::CONNECTION_ETHERNET ||
          interface.type == net::NetworkChangeNotifier::CONNECTION_WIFI ||
          IsMLan(interface));
}

NetBiosHostLocator::NetBiosHostLocator(GetInterfacesFunction get_interfaces,
                                       NetBiosClientFactory client_factory,
                                       SmbProviderClient* smb_provider_client)
    : NetBiosHostLocator(get_interfaces,
                         client_factory,
                         smb_provider_client,
                         std::make_unique<base::OneShotTimer>()) {}

NetBiosHostLocator::NetBiosHostLocator(
    GetInterfacesFunction get_interfaces,
    NetBiosClientFactory client_factory,
    SmbProviderClient* smb_provider_client,
    std::unique_ptr<base::OneShotTimer> timer)
    : get_interfaces_(std::move(get_interfaces)),
      client_factory_(std::move(client_factory)),
      smb_provider_client_(std::move(smb_provider_client)),
      timer_(std::move(timer)) {}

NetBiosHostLocator::~NetBiosHostLocator() = default;

void NetBiosHostLocator::FindHosts(FindHostsCallback callback) {
  DCHECK(!running_);
  DCHECK(callback);
  callback_ = std::move(callback);
  running_ = true;

  net::NetworkInterfaceList network_interface_list = GetNetworkInterfaceList();

  for (const auto& interface : network_interface_list) {
    if (ShouldUseInterface(interface)) {
      FindHostsOnInterface(interface);
    }
  }

  if (netbios_clients_.empty()) {
    // No NetBiosClients were created since there were either no interfaces or
    // no valid interfaces.
    running_ = false;
    std::move(callback_).Run(false /* success */, results_);
    return;
  }

  timer_->Start(FROM_HERE, base::Seconds(kNetBiosDiscoveryTimeoutSeconds), this,
                &NetBiosHostLocator::StopDiscovery);
}

net::NetworkInterfaceList NetBiosHostLocator::GetNetworkInterfaceList() {
  return get_interfaces_.Run();
}

void NetBiosHostLocator::FindHostsOnInterface(
    const net::NetworkInterface& interface) {
  net::IPAddress broadcast_address = CalculateBroadcastAddress(interface);

  netbios_clients_.push_back(CreateClient());
  ExecuteNameRequest(broadcast_address);
}

std::unique_ptr<NetBiosClientInterface> NetBiosHostLocator::CreateClient()
    const {
  return client_factory_.Run();
}

void NetBiosHostLocator::ExecuteNameRequest(
    const net::IPAddress& broadcast_address) {
  netbios_clients_.back()->ExecuteNameRequest(
      broadcast_address, transaction_id_++,
      base::BindRepeating(&NetBiosHostLocator::PacketReceived,
                          base::Unretained(this)));
}

void NetBiosHostLocator::PacketReceived(const std::vector<uint8_t>& packet,
                                        uint16_t transaction_id,
                                        const net::IPEndPoint& sender_ip) {
  if (discovery_done_) {
    // Avoids race condition where this callback is called after the timer has
    // expired.
    return;
  }

  ++outstanding_parse_requests_;
  smb_provider_client_->ParseNetBiosPacket(
      packet, transaction_id,
      base::BindOnce(&NetBiosHostLocator::OnPacketParsed,
                     weak_ptr_factory_.GetWeakPtr(), sender_ip));
}

void NetBiosHostLocator::OnPacketParsed(
    const net::IPEndPoint& sender_ip,
    const std::vector<std::string>& hostnames) {
  DCHECK_GE(outstanding_parse_requests_, 0);

  --outstanding_parse_requests_;
  for (const auto& hostname : hostnames) {
    AddHostToResult(sender_ip, hostname);
  }

  if (discovery_done_ && outstanding_parse_requests_ == 0) {
    FinishFindHosts();
  }
}

void NetBiosHostLocator::StopDiscovery() {
  DCHECK(!discovery_done_);

  discovery_done_ = true;
  netbios_clients_.clear();

  if (outstanding_parse_requests_ == 0) {
    FinishFindHosts();
  }
}

void NetBiosHostLocator::FinishFindHosts() {
  std::move(callback_).Run(true /* success */, results_);
  ResetHostLocator();
}

void NetBiosHostLocator::ResetHostLocator() {
  DCHECK_EQ(0, outstanding_parse_requests_);
  DCHECK(netbios_clients_.empty());

  results_.clear();
  discovery_done_ = false;
  running_ = false;
}

void NetBiosHostLocator::AddHostToResult(const net::IPEndPoint& sender_ip,
                                         const std::string& hostname) {
  if (WouldOverwriteResult(sender_ip, hostname)) {
    LOG(ERROR) << hostname << ":" << results_[hostname].ToString()
               << " will be overwritten by " << hostname << ":"
               << sender_ip.ToStringWithoutPort();
  }
  results_[hostname] = sender_ip.address();
}

bool NetBiosHostLocator::WouldOverwriteResult(
    const net::IPEndPoint& sender_ip,
    const std::string& hostname) const {
  return results_.count(hostname) &&
         results_.at(hostname) != sender_ip.address();
}

}  // namespace ash::smb_client
