// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_

#include <deque>
#include <memory>

#include "chrome/browser/chromeos/net/network_diagnostics/fake_host_resolver.h"
#include "chrome/browser/chromeos/net/network_diagnostics/fake_tcp_connected_socket.h"
#include "services/network/test/test_network_context.h"

namespace chromeos {
namespace network_diagnostics {

// Used in unit tests, the FakeNetworkContext class simulates the behavior of a
// network context.
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext();
  ~FakeNetworkContext() override;

  // network::TestNetworkContext:
  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override;

  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override;

  // Sets the fake TCP connect code.
  void SetTCPConnectCode(base::Optional<net::Error>& tcp_connect_code);

  // Sets the fake TLS upgrade code.
  void SetTLSUpgradeCode(base::Optional<net::Error>& tls_upgrade_code);

  // Sets the fake dns results.
  void set_fake_dns_results(
      std::deque<FakeHostResolver::DnsResult*> fake_dns_results) {
    fake_dns_results_ = std::move(fake_dns_results);
  }

  // If set to true, the binding pipe will be disconnected when attempting to
  // connect.
  void set_disconnect_during_host_resolution(bool disconnect) {
    host_resolution_disconnect_ = disconnect;
  }

  // If set to true, the binding pipe will be disconnected when attempting to
  // connect.
  void set_disconnect_during_tcp_connection_attempt(bool disconnect) {
    tcp_connection_attempt_disconnect_ = disconnect;
  }

  // If set to true, the binding pipe will be disconnected when attempting to
  // connect.
  void set_disconnect_during_tls_upgrade_attempt(bool disconnect) {
    tls_upgrade_attempt_disconnect_ = disconnect;
  }

 private:
  // Fake host resolver.
  std::unique_ptr<FakeHostResolver> resolver_;
  // Fake DNS lookup results.
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results_;
  // Provides the TCP socket functionality for tests.
  std::unique_ptr<FakeTCPConnectedSocket> fake_tcp_connected_socket_;
  // TCP connect code corresponding to the connection attempt.
  net::Error tcp_connect_code_ = net::OK;
  // Used to mimic the scenario where network::mojom::HostResolver receiver
  // is disconnected.
  bool host_resolution_disconnect_ = false;
  // Used to mimic the scenario where network::mojom::TCPConnectedSocket
  // receiver is disconnected.
  bool tcp_connection_attempt_disconnect_ = false;
  // Used to mimic the scenario where network::mojom::TLSClientSocket receiver
  // is disconnected.
  bool tls_upgrade_attempt_disconnect_ = false;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_
