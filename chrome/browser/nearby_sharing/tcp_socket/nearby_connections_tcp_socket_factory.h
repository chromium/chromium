// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TCP_SOCKET_NEARBY_CONNECTIONS_TCP_SOCKET_FACTORY_H_
#define CHROME_BROWSER_NEARBY_SHARING_TCP_SOCKET_NEARBY_CONNECTIONS_TCP_SOCKET_FACTORY_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace ash {
namespace nearby {
class TcpServerSocketPort;
}  // namespace nearby
}  // namespace ash

namespace net {
class AddressList;
struct MutableNetworkTrafficAnnotationTag;
class IPAddress;
}  // namespace net

// An implementation of the mojo service used to create TCP sockets for the
// Nearby Connections WifiLan medium. We guarantee that callbacks will not be
// invoked after this class is destroyed.
class NearbyConnectionsTcpSocketFactory
    : public sharing::mojom::TcpSocketFactory {
 public:

  // A class used to run NetworkContext::CreateTCPConnectedSocket with a
  // user-defined |timeout|. There is timeout logic in the networking stack, but
  // it is not configurable and can be too long in practice (over 2 minutes).
  // The networking stack timeout can still be triggered if |timeout| is too
  // long.
  class ConnectTask {
   public:
    ConnectTask(
        network::mojom::NetworkContext* network_context,
        const std::optional<net::IPEndPoint>& local_addr,
        const net::AddressList& remote_addr_list,
        network::mojom::TCPConnectedSocketOptionsPtr
            tcp_connected_socket_options,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
        mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
        mojo::PendingRemote<network::mojom::SocketObserver> observer,
        CreateTCPConnectedSocketCallback callback);
    ~ConnectTask();

    // Can only be called once.
    void Run(base::TimeDelta timeout);

   private:
    void OnFinished(int32_t result,
                    const std::optional<net::IPEndPoint>& local_addr,
                    const std::optional<net::IPEndPoint>& peer_addr,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream);
    void OnTimeout();

    CreateTCPConnectedSocketCallback callback_;
    base::OnceClosure task_;
    base::OneShotTimer timer_;
    base::TimeTicks start_time_;
    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<ConnectTask> weak_ptr_factory_{this};
  };

  explicit NearbyConnectionsTcpSocketFactory(
      network::NetworkContextGetter network_context_getter);
  NearbyConnectionsTcpSocketFactory(const NearbyConnectionsTcpSocketFactory&) =
      delete;
  NearbyConnectionsTcpSocketFactory& operator=(
      const NearbyConnectionsTcpSocketFactory&) = delete;
  ~NearbyConnectionsTcpSocketFactory() override;

  // sharing::mojom:TcpSocketFactory:
  void CreateTCPServerSocket(
      const net::IPAddress& local_addr,
      const ash::nearby::TcpServerSocketPort& port,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPServerSocket> receiver,
      CreateTCPServerSocketCallback callback) override;
  void CreateTCPConnectedSocket(
      base::TimeDelta timeout,
      const std::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override;

 private:
  // Wrapper callbacks that are bound with weak pointers. Used to guarantee that
  // input callbacks are not invoked after this class is destroyed.
  void OnTcpServerSocketCreated(
      CreateTCPServerSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr);
  void OnTcpConnectedSocketCreated(
      base::UnguessableToken task_id,
      CreateTCPConnectedSocketCallback callback,
      int32_t result,
      const std::optional<net::IPEndPoint>& local_addr,
      const std::optional<net::IPEndPoint>& peer_addr,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);

  network::NetworkContextGetter network_context_getter_;
  base::flat_map<base::UnguessableToken, std::unique_ptr<ConnectTask>>
      connect_tasks_;
  base::WeakPtrFactory<NearbyConnectionsTcpSocketFactory> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TCP_SOCKET_NEARBY_CONNECTIONS_TCP_SOCKET_FACTORY_H_
