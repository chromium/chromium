// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/fake_network_context.h"

#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace ash::network_diagnostics {

namespace {

// Represents a fake port number of a fake IP address.
const int kFakePortNumber = 1234;

// Returns a fake IP address.
net::IPEndPoint FakeIPAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), kFakePortNumber);
}

}  // namespace

FakeNetworkContext::DnsResult::DnsResult(
    int32_t result,
    net::ResolveErrorInfo resolve_error_info,
    std::optional<net::AddressList> resolved_addresses,
    std::optional<net::HostResolverEndpointResults>
        endpoint_results_with_metadata)
    : result_(result),
      resolve_error_info_(resolve_error_info),
      resolved_addresses_(resolved_addresses),
      endpoint_results_with_metadata_(endpoint_results_with_metadata) {}

FakeNetworkContext::DnsResult::~DnsResult() = default;

FakeNetworkContext::FakeNetworkContext() = default;

FakeNetworkContext::~FakeNetworkContext() = default;

void FakeNetworkContext::ResolveHost(
    network::mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient> response_client) {
  if (host_resolution_disconnect_) {
    response_client.reset();
    return;
  }
  mojo::Remote<network::mojom::ResolveHostClient> rpc(
      std::move(response_client));
  if (fake_dns_result_) {
    rpc->OnComplete(fake_dns_result_->result_,
                    fake_dns_result_->resolve_error_info_,
                    fake_dns_result_->resolved_addresses_,
                    fake_dns_result_->endpoint_results_with_metadata_);
  } else {
    CHECK(!fake_dns_results_.empty());
    auto dns_result = std::move(fake_dns_results_.front());
    fake_dns_results_.pop_front();
    rpc->OnComplete(dns_result->result_, dns_result->resolve_error_info_,
                    dns_result->resolved_addresses_,
                    dns_result->endpoint_results_with_metadata_);
  }
  rpc.reset();
}

void FakeNetworkContext::CreateTCPConnectedSocket(
    const std::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    CreateTCPConnectedSocketCallback callback) {
  if (tcp_connection_attempt_disconnect_) {
    socket.reset();
    return;
  }

  // Only bind the receiver if TCP connection is successful.
  if (tcp_connect_code_ == net::OK) {
    DCHECK(fake_tcp_connected_socket_);
    fake_tcp_connected_socket_->BindReceiver(std::move(socket));
    fake_tcp_connected_socket_->set_disconnect_during_tls_upgrade_attempt(
        tls_upgrade_attempt_disconnect_);
  }
  std::move(callback).Run(tcp_connect_code_, FakeIPAddress(), FakeIPAddress(),
                          mojo::ScopedDataPipeConsumerHandle(),
                          mojo::ScopedDataPipeProducerHandle());
}

void FakeNetworkContext::CreateUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  if (udp_connection_attempt_disconnect_) {
    receiver.reset();
    listener.reset();
    return;
  }

  // Bind the receiver if UDP connection is successful.
  if (udp_connect_code_ == net::OK) {
    DCHECK(fake_udp_socket_);

    fake_udp_socket_->BindReceiver(std::move(receiver));
    fake_udp_socket_->BindRemote(std::move(listener));
  }
}

void FakeNetworkContext::SetTCPConnectCode(
    std::optional<net::Error>& tcp_connect_code) {
  if (tcp_connect_code.has_value()) {
    tcp_connect_code_ = tcp_connect_code.value();
    fake_tcp_connected_socket_ = std::make_unique<FakeTCPConnectedSocket>();
  }
}

void FakeNetworkContext::SetTLSUpgradeCode(
    std::optional<net::Error>& tls_upgrade_code) {
  if (tls_upgrade_code.has_value()) {
    DCHECK(fake_tcp_connected_socket_);

    fake_tcp_connected_socket_->set_tls_upgrade_code(tls_upgrade_code.value());
  }
}

void FakeNetworkContext::SetUdpConnectCode(net::Error udp_connect_code) {
  fake_udp_socket_ = std::make_unique<FakeUdpSocket>();
  fake_udp_socket_->set_udp_connect_code(udp_connect_code);
}

void FakeNetworkContext::SetUdpSendCode(net::Error udp_send_code) {
  DCHECK(fake_udp_socket_);

  fake_udp_socket_->set_udp_send_code(udp_send_code);
}

void FakeNetworkContext::SetDisconnectDuringUdpSendAttempt(bool disconnect) {
  DCHECK(fake_udp_socket_);

  fake_udp_socket_->set_disconnect_during_udp_send_attempt(disconnect);
}

void FakeNetworkContext::SetUdpOnReceivedCode(net::Error udp_on_received_code) {
  DCHECK(fake_udp_socket_);

  fake_udp_socket_->set_udp_on_received_code(udp_on_received_code);
}

void FakeNetworkContext::SetUdpOnReceivedData(
    base::span<const uint8_t> udp_on_received_data) {
  DCHECK(fake_udp_socket_);

  fake_udp_socket_->set_udp_on_received_data(std::move(udp_on_received_data));
}

void FakeNetworkContext::SetDisconnectDuringUdpReceiveAttempt(bool disconnect) {
  DCHECK(fake_udp_socket_);

  fake_udp_socket_->set_disconnect_during_udp_receive_attempt(disconnect);
}

void FakeNetworkContext::SetTaskEnvironmentForTesting(
    content::BrowserTaskEnvironment* task_environment) {
  fake_udp_socket_->set_task_environment_for_testing(task_environment);
}

void FakeNetworkContext::SetUdpConnectionDelay(
    base::TimeDelta connection_delay) {
  fake_udp_socket_->set_udp_connection_delay(connection_delay);
}

void FakeNetworkContext::SetUdpSendDelay(base::TimeDelta send_delay) {
  fake_udp_socket_->set_udp_send_delay(send_delay);
}

void FakeNetworkContext::SetUdpReceiveDelay(base::TimeDelta receive_delay) {
  fake_udp_socket_->set_udp_receive_delay(receive_delay);
}

}  // namespace ash::network_diagnostics
