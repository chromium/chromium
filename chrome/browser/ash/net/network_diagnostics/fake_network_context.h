// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_tcp_connected_socket.h"
#include "chrome/browser/ash/net/network_diagnostics/fake_udp_socket.h"
#include "services/network/test/test_network_context.h"

namespace ash::network_diagnostics {

// Used in unit tests, the FakeNetworkContext class simulates the behavior of a
// network context.
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  struct DnsResult {
   public:
    DnsResult(int32_t result,
              net::ResolveErrorInfo resolve_error_info,
              std::optional<net::AddressList> resolved_addresses,
              std::optional<net::HostResolverEndpointResults>
                  endpoint_results_with_metadata);
    ~DnsResult();

    int result_;
    net::ResolveErrorInfo resolve_error_info_;
    std::optional<net::AddressList> resolved_addresses_;
    std::optional<net::HostResolverEndpointResults>
        endpoint_results_with_metadata_;
  };
  FakeNetworkContext();
  FakeNetworkContext(const FakeNetworkContext&) = delete;
  FakeNetworkContext& operator=(const FakeNetworkContext&) = delete;
  ~FakeNetworkContext() override;

  // network::TestNetworkContext:
  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient> response_client)
      override;

  void CreateTCPConnectedSocket(
      const std::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override;

  void CreateUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener) override;

  // Sets the fake TCP connect code. TODO(khegde): Change this to
  // SetTCPConnectCompleteCode.
  void SetTCPConnectCode(std::optional<net::Error>& tcp_connect_code);

  // Sets the fake TLS upgrade code.
  void SetTLSUpgradeCode(std::optional<net::Error>& tls_upgrade_code);

  // Sets the fake UDP connect code.
  void SetUdpConnectCode(net::Error udp_connect_code);

  // Sets the fake UDP send code.
  void SetUdpSendCode(net::Error udp_send_code);

  // Sets the state to mimic a fake disconnect during a UDP send attempt.
  void SetDisconnectDuringUdpSendAttempt(bool disconnect);

  // Sets the fake UDP on received code.
  void SetUdpOnReceivedCode(net::Error udp_on_received_code);

  // Sets the fake UDP on received data.
  void SetUdpOnReceivedData(base::span<const uint8_t> udp_on_received_data);

  // Sets the state to mimic a fake disconnect after receiving successful send
  // confirmation, but before receiving any data.
  void SetDisconnectDuringUdpReceiveAttempt(bool disconnect);

  // Sets the task environment used in testing. Used to fast forward the clock.
  void SetTaskEnvironmentForTesting(
      content::BrowserTaskEnvironment* task_environment);

  // Sets the UDP connection delay.
  void SetUdpConnectionDelay(base::TimeDelta connection_delay);

  // Sets the UDP send delay.
  void SetUdpSendDelay(base::TimeDelta send_delay);

  // Sets the UDP receive delay.
  void SetUdpReceiveDelay(base::TimeDelta receive_delay);

  // Sets the fake DNS result. Used to test a single host resolution.
  void set_fake_dns_result(std::unique_ptr<DnsResult> fake_dns_result) {
    CHECK(fake_dns_results_.empty());
    fake_dns_result_ = std::move(fake_dns_result);
  }

  // Sets the deque of fake DNS results. Used to test a sequence of host
  // resolutions.
  void set_fake_dns_results(
      base::circular_deque<std::unique_ptr<DnsResult>> fake_dns_results) {
    CHECK(!fake_dns_result_);
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

  // If set to true, the binding pipe will be disconnected when attempting to
  // connect.
  void set_disconnect_during_udp_connection_attempt(bool disconnect) {
    udp_connection_attempt_disconnect_ = disconnect;
  }

 private:
  // Fake DNS lookup result. Persists across resolutions.
  // Cannot be set together with |fake_dns_results_|.
  std::unique_ptr<DnsResult> fake_dns_result_;
  // Fake DNS lookup results -- for every query the resolver pops and returns
  // the front entry.
  // Cannot be set together with |fake_dns_result_|.
  base::circular_deque<std::unique_ptr<DnsResult>> fake_dns_results_;
  // Provides the TCP socket functionality for tests.
  std::unique_ptr<FakeTCPConnectedSocket> fake_tcp_connected_socket_;
  // Provides the UDP socket functionality for tests.
  std::unique_ptr<FakeUdpSocket> fake_udp_socket_;
  // TCP connect code corresponding to the connection attempt.
  net::Error tcp_connect_code_ = net::OK;
  // UDP connect code corresponding to the connection attempt.
  net::Error udp_connect_code_ = net::OK;

  // Used to mimic the scenario where network::mojom::HostResolver receiver
  // is disconnected.
  bool host_resolution_disconnect_ = false;
  // Used to mimic the scenario where network::mojom::TCPConnectedSocket
  // receiver is disconnected.
  bool tcp_connection_attempt_disconnect_ = false;
  // Used to mimic the scenario where network::mojom::TLSClientSocket receiver
  // is disconnected.
  bool tls_upgrade_attempt_disconnect_ = false;
  // Used to mimic the scenario where network::mojom::UDPSocket receiver is
  // disconnected while connecting.
  bool udp_connection_attempt_disconnect_ = false;
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_
