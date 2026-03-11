// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_UDP_SOCKET_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_UDP_SOCKET_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace ash {
namespace network_diagnostics {

// Provides some UDP socket functionality in tests. Most methods, unless
// otherwise noted, are not expected to be called in tests or in a non Chrome OS
// environment.
class FakeUdpSocket : public network::mojom::UDPSocket {
 public:
  FakeUdpSocket();
  FakeUdpSocket(const FakeUdpSocket&) = delete;
  FakeUdpSocket& operator=(const FakeUdpSocket&) = delete;
  ~FakeUdpSocket() override;

  // network::mojom::UDPSocket:
  // Used in the fake.
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override;
  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override;
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override;
  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetSendBufferSizeCallback callback) override;
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override;
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override;
  // Used in the fake.
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override;
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override;
  // Used in the fake.
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;
  void Close() override;

  // Binds the pending receiver to |this|.
  void BindReceiver(mojo::PendingReceiver<network::mojom::UDPSocket> socket);

  // Binds the pending remote to a remote held by |this|.
  void BindRemote(
      mojo::PendingRemote<network::mojom::UDPSocketListener> socket_listener);

  void set_udp_connect_code(net::Error udp_connect_code) {
    udp_connect_code_ = udp_connect_code;
  }

  void set_udp_send_code(net::Error udp_send_code) {
    udp_send_code_ = udp_send_code;
  }

  void set_udp_on_received_code(net::Error udp_on_received_code) {
    udp_on_received_code_ = udp_on_received_code;
  }

  void set_udp_on_received_data(
      base::span<const uint8_t> udp_on_received_data) {
    udp_on_received_data_ = std::move(udp_on_received_data);
  }

  void set_disconnect_during_udp_connection_attempt(bool disconnect) {
    mojo_disconnect_on_connect_ = disconnect;
  }

  void set_disconnect_during_udp_send_attempt(bool disconnect) {
    mojo_disconnect_on_send_ = disconnect;
  }

  void set_disconnect_during_udp_receive_attempt(bool disconnect) {
    mojo_disconnect_on_receive_ = disconnect;
  }

  void set_task_environment_for_testing(
      content::BrowserTaskEnvironment* task_environment) {
    task_environment_ = task_environment;
  }

  void set_udp_connection_delay(base::TimeDelta connection_delay) {
    connection_delay_ = connection_delay;
  }

  void set_udp_send_delay(base::TimeDelta send_delay) {
    send_delay_ = send_delay;
  }

  void set_udp_receive_delay(base::TimeDelta receive_delay) {
    receive_delay_ = receive_delay;
  }

 private:
  mojo::Receiver<network::mojom::UDPSocket> receiver_{this};
  mojo::Remote<network::mojom::UDPSocketListener> remote_;
  net::Error udp_connect_code_ = net::ERR_FAILED;
  net::Error udp_send_code_ = net::ERR_FAILED;
  net::Error udp_on_received_code_ = net::ERR_FAILED;
  base::raw_span<const uint8_t> udp_on_received_data_ = {};
  bool mojo_disconnect_on_connect_ = false;
  bool mojo_disconnect_on_send_ = false;
  bool mojo_disconnect_on_receive_ = false;
  raw_ptr<content::BrowserTaskEnvironment> task_environment_;
  base::TimeDelta connection_delay_;
  base::TimeDelta send_delay_;
  base::TimeDelta receive_delay_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_UDP_SOCKET_H_
