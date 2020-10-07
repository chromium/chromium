// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/fake_network_context.h"

#include <utility>

namespace chromeos {
namespace network_diagnostics {

namespace {

// Represents a fake port number of a fake IP address.
const int kFakePortNumber = 1234;

// Returns a fake IP address.
net::IPEndPoint FakeIPAddress() {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), kFakePortNumber);
}

}  // namespace

FakeNetworkContext::FakeNetworkContext() = default;

FakeNetworkContext::~FakeNetworkContext() = default;

void FakeNetworkContext::CreateHostResolver(
    const base::Optional<net::DnsConfigOverrides>& config_overrides,
    mojo::PendingReceiver<network::mojom::HostResolver> receiver) {
  DCHECK(!resolver_);
  resolver_ = std::make_unique<FakeHostResolver>(std::move(receiver));
  resolver_->set_fake_dns_results(std::move(fake_dns_results_));
  resolver_->set_disconnect_during_host_resolution(host_resolution_disconnect_);
}

void FakeNetworkContext::CreateTCPConnectedSocket(
    const base::Optional<net::IPEndPoint>& local_addr,
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

void FakeNetworkContext::SetTCPConnectCode(
    base::Optional<net::Error>& tcp_connect_code) {
  if (tcp_connect_code.has_value()) {
    tcp_connect_code_ = tcp_connect_code.value();
  }
}

void FakeNetworkContext::SetTLSUpgradeCode(
    base::Optional<net::Error>& tls_upgrade_code) {
  if (tls_upgrade_code.has_value()) {
    fake_tcp_connected_socket_ = std::make_unique<FakeTCPConnectedSocket>();
    fake_tcp_connected_socket_->set_tls_upgrade_code(tls_upgrade_code.value());
  }
}

}  // namespace network_diagnostics
}  // namespace chromeos
