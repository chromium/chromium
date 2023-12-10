// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/fake_tcp_connected_socket.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace ash {

FakeTCPConnectedSocket::FakeTCPConnectedSocket() = default;

FakeTCPConnectedSocket::~FakeTCPConnectedSocket() = default;

void FakeTCPConnectedSocket::UpgradeToTLS(
    const net::HostPortPair& host_port_pair,
    network::mojom::TLSClientSocketOptionsPtr socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::TLSClientSocket> receiver,
    mojo::PendingRemote<network::mojom::SocketObserver> observer,
    network::mojom::TCPConnectedSocket::UpgradeToTLSCallback callback) {
  if (disconnect_) {
    receiver_.reset();
    return;
  }
  // Only during no disconnects will |callback| be invoked.
  std::move(callback).Run(tls_upgrade_code_,
                          mojo::ScopedDataPipeConsumerHandle(),
                          mojo::ScopedDataPipeProducerHandle(),
                          /*ssl_info=*/std::nullopt);
}

void FakeTCPConnectedSocket::SetSendBufferSize(
    int32_t send_buffer_size,
    SetSendBufferSizeCallback callback) {
  std::move(callback).Run(0);
}

void FakeTCPConnectedSocket::SetReceiveBufferSize(
    int32_t receive_buffer_size,
    SetReceiveBufferSizeCallback callback) {
  std::move(callback).Run(0);
}

void FakeTCPConnectedSocket::SetNoDelay(bool no_delay,
                                        SetNoDelayCallback callback) {
  std::move(callback).Run(false);
}
void FakeTCPConnectedSocket::SetKeepAlive(bool enable,
                                          int32_t delay_secs,
                                          SetKeepAliveCallback callback) {
  std::move(callback).Run(0);
}

void FakeTCPConnectedSocket::BindReceiver(
    mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket) {
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(socket));
}

}  // namespace ash
