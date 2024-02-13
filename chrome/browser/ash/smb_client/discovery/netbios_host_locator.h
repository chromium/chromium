// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_

#include <list>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/smb_client/discovery/host_locator.h"
#include "chrome/browser/ash/smb_client/discovery/netbios_client_interface.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"
#include "net/base/network_interfaces.h"

namespace ash::smb_client {

// Calculates the broadcast address of a network interface.
net::IPAddress CalculateBroadcastAddress(
    const net::NetworkInterface& interface);

// Returns true if a network interface should be used for NetBios discovery.
bool ShouldUseInterface(const net::NetworkInterface& interface);

// HostLocator implementation that uses NetBIOS to locate hosts.
class NetBiosHostLocator final : public HostLocator {
 public:
  using GetInterfacesFunction =
      base::RepeatingCallback<net::NetworkInterfaceList()>;
  using NetBiosClientFactory =
      base::RepeatingCallback<std::unique_ptr<NetBiosClientInterface>()>;

  NetBiosHostLocator(GetInterfacesFunction get_interfaces,
                     NetBiosClientFactory client_factory,
                     SmbProviderClient* smb_provider_client);
  NetBiosHostLocator(GetInterfacesFunction get_interfaces,
                     NetBiosClientFactory client_factory,
                     SmbProviderClient* smb_provider_client,
                     std::unique_ptr<base::OneShotTimer> timer);

  NetBiosHostLocator(const NetBiosHostLocator&) = delete;
  NetBiosHostLocator& operator=(const NetBiosHostLocator&) = delete;

  ~NetBiosHostLocator() override;

  // HostLocator override.
  void FindHosts(FindHostsCallback callback) override;

 private:
  // Returns a list of network interfaces on the device.
  net::NetworkInterfaceList GetNetworkInterfaceList();

  // Finds hosts on |interface| by constructing a NetBiosClient, performing a
  // NetBios Name Request for the interface.
  void FindHostsOnInterface(const net::NetworkInterface& interface);

  // Creates a NetBiosClient using the |client_factory_|.
  std::unique_ptr<NetBiosClientInterface> CreateClient() const;

  // Executes a name request transaction for |broadcast_address| using the most
  // recently added NetBiosClient in |netbios_clients_|.
  void ExecuteNameRequest(const net::IPAddress& broadcast_address);

  // Callback handler for packets received by the |netbios_clients_|.
  void PacketReceived(const std::vector<uint8_t>& packet,
                      uint16_t transaction_id,
                      const net::IPEndPoint& sender_ip);

  // Callback handler for a request to parse a packet. Adds
  // <hostname, sender_ip> entries to |results_|.
  void OnPacketParsed(const net::IPEndPoint& sender_ip,
                      const std::vector<std::string>& hostnames);

  // Called upon expiration of |timer_|. Deletes all active netbios clients. If
  // there are no |outstanding_parse_requests_|, FinishFindHosts is called which
  // returns the results to the NetworkScanner.
  void StopDiscovery();

  // Runs |callback_| with |results_|, then calls ResetHostLocator to reset the
  // state.
  void FinishFindHosts();

  // Resets the state of the HostLocator so that it can be resued.
  void ResetHostLocator();

  // Helper function to add a <hostname, sender_ip> pair to |results_|.
  void AddHostToResult(const net::IPEndPoint& sender_ip,
                       const std::string& hostname);

  // Checks whether an entry already exists for |hostname| in |results_| with a
  // different |sender_ip|.
  bool WouldOverwriteResult(const net::IPEndPoint& sender_ip,
                            const std::string& hostname) const;

  bool running_ = false;
  bool discovery_done_ = false;
  uint16_t transaction_id_ = 0;
  int32_t outstanding_parse_requests_ = 0;
  GetInterfacesFunction get_interfaces_;
  NetBiosClientFactory client_factory_;
  raw_ptr<SmbProviderClient> smb_provider_client_;
  FindHostsCallback callback_;
  HostMap results_;
  // |netbios_clients_| is a container for storing NetBios clients that are
  // currently performing a NetBios Name Request so that they do not go out of
  // scope. One NetBiosClient exists for each network interface on the device.
  std::list<std::unique_ptr<NetBiosClientInterface>> netbios_clients_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::WeakPtrFactory<NetBiosHostLocator> weak_ptr_factory_{this};
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_DISCOVERY_NETBIOS_HOST_LOCATOR_H_
