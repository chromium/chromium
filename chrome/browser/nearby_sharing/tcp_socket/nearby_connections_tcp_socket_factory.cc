// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/tcp_socket/nearby_connections_tcp_socket_factory.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

NearbyConnectionsTcpSocketFactory::ConnectTask::ConnectTask(
    network::mojom::NetworkContext* network_context,
    const std::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    CreateTCPConnectedSocketCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(network_context);
  task_ = base::BindOnce(
      &network::mojom::NetworkContext::CreateTCPConnectedSocket,
      base::Unretained(network_context), local_addr, remote_addr_list,
      std::move(tcp_connected_socket_options), traffic_annotation,
      std::move(receiver), std::move(observer),
      base::BindOnce(&ConnectTask::OnFinished, weak_ptr_factory_.GetWeakPtr()));
}

NearbyConnectionsTcpSocketFactory::ConnectTask::~ConnectTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NearbyConnectionsTcpSocketFactory::ConnectTask::Run(
    base::TimeDelta timeout) {
  timer_.Start(FROM_HERE, timeout,
               base::BindOnce(&ConnectTask::OnTimeout, base::Unretained(this)));
  start_time_ = base::TimeTicks::Now();
  std::move(task_).Run();
}

void NearbyConnectionsTcpSocketFactory::ConnectTask::OnFinished(
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
  if (result == net::OK) {
    base::UmaHistogramTimes("Nearby.Connections.WifiLan.TimeToConnect",
                            base::TimeTicks::Now() - start_time_);
  }

  // Just to be safe, protect against finish/timeout race conditions.
  if (!callback_)
    return;

  std::move(callback_).Run(result, local_addr, peer_addr,
                           std::move(receive_stream), std::move(send_stream));
}

void NearbyConnectionsTcpSocketFactory::ConnectTask::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  OnFinished(net::ERR_TIMED_OUT, /*local_addr=*/std::nullopt,
             /*peer_addr=*/std::nullopt,
             /*receive_stream=*/mojo::ScopedDataPipeConsumerHandle(),
             /*send_stream=*/mojo::ScopedDataPipeProducerHandle());
}

NearbyConnectionsTcpSocketFactory::NearbyConnectionsTcpSocketFactory(
    network::NetworkContextGetter network_context_getter)
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
    std::move(callback).Run(net::ERR_FAILED, /*local_addr=*/std::nullopt);
    return;
  }
  auto options = network::mojom::TCPServerSocketOptions::New();
  options->backlog = backlog;
  network_context->CreateTCPServerSocket(
      net::IPEndPoint(local_addr, port.port()), std::move(options),
      traffic_annotation, std::move(receiver),
      base::BindOnce(
          &NearbyConnectionsTcpSocketFactory::OnTcpServerSocketCreated,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NearbyConnectionsTcpSocketFactory::CreateTCPConnectedSocket(
    base::TimeDelta timeout,
    const std::optional<net::IPEndPoint>& local_addr,
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
        net::ERR_FAILED, /*local_addr=*/std::nullopt,
        /*peer_addr=*/std::nullopt,
        /*receive_stream=*/mojo::ScopedDataPipeConsumerHandle(),
        /*send_stream=*/mojo::ScopedDataPipeProducerHandle());
    return;
  }

  base::UnguessableToken task_id = base::UnguessableToken::Create();
  connect_tasks_.insert_or_assign(
      task_id,
      std::make_unique<ConnectTask>(
          network_context, local_addr, remote_addr_list,
          std::move(tcp_connected_socket_options), traffic_annotation,
          std::move(receiver), std::move(observer),
          base::BindOnce(
              &NearbyConnectionsTcpSocketFactory::OnTcpConnectedSocketCreated,
              base::Unretained(this), task_id, std::move(callback))));
  connect_tasks_[task_id]->Run(timeout);
}

void NearbyConnectionsTcpSocketFactory::OnTcpServerSocketCreated(
    CreateTCPServerSocketCallback callback,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr) {
  std::move(callback).Run(result, local_addr);
}

void NearbyConnectionsTcpSocketFactory::OnTcpConnectedSocketCreated(
    base::UnguessableToken task_id,
    CreateTCPConnectedSocketCallback callback,
    int32_t result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  std::move(callback).Run(result, local_addr, peer_addr,
                          std::move(receive_stream), std::move(send_stream));
  DCHECK(base::Contains(connect_tasks_, task_id));
  connect_tasks_.erase(task_id);
}
