// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_NETWORK_SERVICE_ASYNC_PACKET_SENDER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_NETWORK_SERVICE_ASYNC_PACKET_SENDER_H_

#include "base/functional/callback.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/ip_endpoint.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace media_router {

class AsyncPacketSender {
 public:
  virtual ~AsyncPacketSender() {}

  virtual net::Error SendTo(const net::IPEndPoint& dest_addr,
                            base::span<const uint8_t> data,
                            base::OnceCallback<void(int32_t)> callback) = 0;
};

class NetworkServiceAsyncPacketSender : public AsyncPacketSender {
 public:
  explicit NetworkServiceAsyncPacketSender(
      network::mojom::NetworkContext* network_context);
  explicit NetworkServiceAsyncPacketSender(NetworkServiceAsyncPacketSender&&);

  NetworkServiceAsyncPacketSender(const NetworkServiceAsyncPacketSender&) =
      delete;
  NetworkServiceAsyncPacketSender& operator=(
      const NetworkServiceAsyncPacketSender&) = delete;

  ~NetworkServiceAsyncPacketSender() override;

  // network::mojom::UDPSocket forwards.
  net::Error SendTo(const net::IPEndPoint& dest_addr,
                    base::span<const uint8_t> data,
                    base::OnceCallback<void(int32_t)> callback) override;

 private:
  mojo::Remote<network::mojom::UDPSocket> socket_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_NETWORK_SERVICE_ASYNC_PACKET_SENDER_H_
