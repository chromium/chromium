// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/discovery/netbios_client_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace net {

class IPEndPoint;

}  // namespace net

namespace network {
namespace mojom {

class NetworkContext;

}  // namespace mojom
}  // namespace network

namespace chromeos {

class FirewallHole;

namespace smb_client {

// NetBiosClient handles a NetBios Name Query Request.
// On construction, the Name Query Request process starts.
//
// Name Query Request Process:
// - A UDP server socket is bound on an open port.
// - A firewall hole is opened on that port.
// - A NetBios Name Query Request Packet is sent to |broadcast_address|
// - Any responses to the NetBios Name Query Request are forwarded to the
//   callback passed in the constructor.
//
// The socket remains open and receives response as long as the instance of this
// class is alive. Upon destruction, the socket and corresponding firewall hole
// are closed.
class NetBiosClient : public network::mojom::UDPSocketListener,
                      public NetBiosClientInterface,
                      public base::SupportsWeakPtr<NetBiosClient> {
 public:
  using NetBiosResponseCallback = base::RepeatingCallback<
      void(const std::vector<uint8_t>&, uint16_t, const net::IPEndPoint&)>;

  explicit NetBiosClient(network::mojom::NetworkContext* network_context);

  ~NetBiosClient() override;

  // NetBiosClientInterface override.
  void ExecuteNameRequest(const net::IPAddress& broadcast_address,
                          uint16_t transaction_id,
                          NetBiosResponseCallback callback) override;

 private:
  // Binds the socket to the wildcard address 0.0.0.0:0
  void BindSocket();

  // Opens a firewall hole for |port| so that response packets can be received.
  void OpenPort(uint16_t port);

  // Sets the socket to allow sending to the broadcast address.
  void SetBroadcast();

  // Creates and sends the NetBios Name Query Response packet.
  void SendPacket();

  // Callback handler for bind. Calls OpenPort.
  void OnBindComplete(int32_t result,
                      const base::Optional<net::IPEndPoint>& local_ip);

  // Callback handler for OpenPort. Calls SetBroadcast.
  void OnOpenPortComplete(std::unique_ptr<FirewallHole> firewall_hole);

  // Callback handler for SetBroadcast. Calls SendPacket.
  void OnSetBroadcastCompleted(int32_t result);

  // Callback handler for SendPacket.
  void OnSendCompleted(int32_t result);

  // network::mojom::UDPSocketListener implementation.
  void OnReceived(int32_t result,
                  const base::Optional<net::IPEndPoint>& src_ip,
                  base::Optional<base::span<const uint8_t>> data) override;

  // Creates a NetBios Name Query Request packet.
  // https://tools.ietf.org/html/rfc1002
  // Section 4.2.12
  std::vector<uint8_t> GenerateBroadcastPacket();

  bool executed_ = false;
  const net::IPEndPoint bind_address_;
  net::IPEndPoint broadcast_address_;
  uint16_t transaction_id_;
  NetBiosResponseCallback callback_;
  std::unique_ptr<FirewallHole> firewall_hole_;
  mojo::Remote<network::mojom::UDPSocket> server_socket_;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(NetBiosClient);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETBIOS_CLIENT_H_
