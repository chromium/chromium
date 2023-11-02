// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_TCP_CONNECTED_SOCKET_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_TCP_CONNECTED_SOCKET_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ash {

class FakeTCPConnectedSocket : public network::mojom::TCPConnectedSocket {
 public:
  FakeTCPConnectedSocket();
  FakeTCPConnectedSocket(const FakeTCPConnectedSocket&) = delete;
  FakeTCPConnectedSocket& operator=(const FakeTCPConnectedSocket&) = delete;
  ~FakeTCPConnectedSocket() override;

  // network::mojom::TCPConnectedSocket:
  void UpgradeToTLS(
      const net::HostPortPair& host_port_pair,
      network::mojom::TLSClientSocketOptionsPtr socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TLSClientSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      network::mojom::TCPConnectedSocket::UpgradeToTLSCallback callback)
      override;
  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetReceiveBufferSizeCallback callback) override;
  void SetNoDelay(bool no_delay, SetNoDelayCallback callback) override;
  void SetKeepAlive(bool enable,
                    int32_t delay_secs,
                    SetKeepAliveCallback callback) override;

  // Binds the pending receiver to |this|.
  void BindReceiver(
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket);

  void set_tls_upgrade_code(net::Error tls_upgrade_code) {
    tls_upgrade_code_ = tls_upgrade_code;
  }

  void set_disconnect_during_tls_upgrade_attempt(bool disconnect) {
    disconnect_ = disconnect;
  }

 private:
  mojo::Receiver<network::mojom::TCPConnectedSocket> receiver_{this};
  net::Error tls_upgrade_code_;
  bool disconnect_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_FAKE_TCP_CONNECTED_SOCKET_H_
