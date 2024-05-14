// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/fake_udp_socket.h"

#include <optional>
#include <utility>

#include "net/base/ip_endpoint.h"

namespace ash {
namespace network_diagnostics {

namespace {}  // namespace

FakeUdpSocket::FakeUdpSocket() = default;

FakeUdpSocket::~FakeUdpSocket() = default;

void FakeUdpSocket::Connect(const net::IPEndPoint& remote_addr,
                            network::mojom::UDPSocketOptionsPtr options,
                            ConnectCallback callback) {
  DCHECK(task_environment_);

  task_environment_->FastForwardBy(connection_delay_);
  if (mojo_disconnect_on_connect_) {
    receiver_.reset();
    return;
  }
  std::move(callback).Run(udp_connect_code_, net::IPEndPoint());
}

void FakeUdpSocket::Bind(const net::IPEndPoint& local_addr,
                         network::mojom::UDPSocketOptionsPtr options,
                         BindCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::SetBroadcast(bool broadcast,
                                 SetBroadcastCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::SetSendBufferSize(int32_t send_buffer_size,
                                      SetSendBufferSizeCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::SetReceiveBufferSize(int32_t receive_buffer_size,
                                         SetSendBufferSizeCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::JoinGroup(const net::IPAddress& group_address,
                              JoinGroupCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::LeaveGroup(const net::IPAddress& group_address,
                               LeaveGroupCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  DCHECK(remote_.is_bound());
  DCHECK(task_environment_);

  task_environment_->FastForwardBy(receive_delay_);
  if (mojo_disconnect_on_receive_) {
    remote_.reset();
    return;
  }

  remote_->OnReceived(udp_on_received_code_, net::IPEndPoint(),
                      std::move(udp_on_received_data_));
}

void FakeUdpSocket::ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                              uint32_t buffer_size) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::SendTo(
    const net::IPEndPoint& dest_addr,
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  DCHECK(task_environment_);

  task_environment_->FastForwardBy(send_delay_);
  if (mojo_disconnect_on_send_) {
    receiver_.reset();
    return;
  }
  std::move(callback).Run(udp_send_code_);
}

void FakeUdpSocket::Close() {
  NOTREACHED_IN_MIGRATION();
}

void FakeUdpSocket::BindReceiver(
    mojo::PendingReceiver<network::mojom::UDPSocket> socket) {
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(socket));
}

void FakeUdpSocket::BindRemote(
    mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener) {
  DCHECK(!remote_.is_bound());

  remote_.Bind(std::move(socket_listener));
}

}  // namespace network_diagnostics
}  // namespace ash
