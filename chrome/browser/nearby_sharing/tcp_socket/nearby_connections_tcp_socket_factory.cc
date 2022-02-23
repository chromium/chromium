// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/tcp_socket/nearby_connections_tcp_socket_factory.h"

#include "ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "base/bind.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

NearbyConnectionsTcpSocketFactory::NearbyConnectionsTcpSocketFactory(
    NetworkContextGetter network_context_getter)
    : network_context_getter_(std::move(network_context_getter)) {}

NearbyConnectionsTcpSocketFactory::~NearbyConnectionsTcpSocketFactory() =
    default;

void NearbyConnectionsTcpSocketFactory::CreateTCPServerSocket(
    const net::IPAddress& local_addr,
    const ash::nearby::TcpServerSocketPort& port,
    uint32_t backlog,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPServerSocket> receiver,
    CreateTCPServerSocketCallback callback) {
  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  if (!network_context) {
    std::move(callback).Run(net::ERR_FAILED, /*local_addr_out=*/absl::nullopt);
    return;
  }

  network_context->CreateTCPServerSocket(
      net::IPEndPoint(local_addr, port.port()), backlog, traffic_annotation,
      std::move(receiver),
      base::BindOnce(
          &NearbyConnectionsTcpSocketFactory::OnTcpServerSocketCreated,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NearbyConnectionsTcpSocketFactory::CreateTCPConnectedSocket(
    const absl::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    CreateTCPConnectedSocketCallback callback) {
  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  if (!network_context) {
    std::move(callback).Run(
        net::ERR_FAILED, /*local_addr=*/absl::nullopt,
        /*peer_addr=*/absl::nullopt,
        /*receive_stream=*/mojo::ScopedDataPipeConsumerHandle(),
        /*send_stream=*/mojo::ScopedDataPipeProducerHandle());
    return;
  }

  network_context->CreateTCPConnectedSocket(
      local_addr, remote_addr_list, std::move(tcp_connected_socket_options),
      traffic_annotation, std::move(receiver), std::move(observer),
      base::BindOnce(
          &NearbyConnectionsTcpSocketFactory::OnTcpConnectedSocketCreated,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NearbyConnectionsTcpSocketFactory::OnTcpServerSocketCreated(
    CreateTCPServerSocketCallback callback,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  std::move(callback).Run(result, local_addr);
}

void NearbyConnectionsTcpSocketFactory::OnTcpConnectedSocketCreated(
    CreateTCPConnectedSocketCallback callback,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr,
    const absl::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  std::move(callback).Run(result, local_addr, peer_addr,
                          std::move(receive_stream), std::move(send_stream));
}
