// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_TLS_PROBER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_TLS_PROBER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace network {
class SimpleHostResolver;
}  // namespace network

namespace ash::network_diagnostics {

// Uses either a TCP or TLS socket to determine whether a socket connection to a
// host can be established. No read or write functionality is exposed by this
// socket. Used by network diagnostics routines.
class TlsProber {
 public:
  // Lists the ways a prober may end. The callback passed into the prober's
  // constructor is invoked while exiting.
  enum ProbeExitEnum {
    kDnsFailure,
    kTcpConnectionFailure,
    kTlsUpgradeFailure,
    kMojoDisconnectFailure,
    kSuccess,
  };
  using OnConnectCompleteOnUIThreadCallback =
      base::OnceCallback<void(int result,
                              const std::optional<net::IPEndPoint>& peer_addr)>;
  using TlsProbeCompleteCallback =
      base::OnceCallback<void(int result, ProbeExitEnum probe_exit_enum)>;

  // Establishes a TCP connection to |host_port_pair|. If |negotiate_tls| is
  // true, the underlying TCP socket upgrades to include TLS support. Note that
  // the constructor will not invoke |callback|, which is passed into TlsProber
  // during construction. This ensures the TlsProber instance is constructed
  // before |callback| is invoked.  The TlsProber must be created on the UI
  // thread and will invoke |callback| on the UI thread.
  // |network_context_getter| will be invoked on the UI thread.
  TlsProber(network::NetworkContextGetter network_context_getter,
            net::HostPortPair host_port_pair,
            bool negotiate_tls,
            TlsProbeCompleteCallback callback);
  TlsProber(const TlsProber&) = delete;
  TlsProber& operator=(const TlsProber&) = delete;
  virtual ~TlsProber();

  // Processes the results of the DNS resolution done by |host_resolver_|.
  void OnHostResolutionComplete(
      int result,
      const net::ResolveErrorInfo&,
      const std::optional<net::AddressList>& resolved_addresses,
      const std::optional<net::HostResolverEndpointResults>&);

 protected:
  // Test-only constructor.
  TlsProber();

 private:
  // On success, upgrades a TCPConnectedSocket to a TLSClientSocket. On failure,
  // invokes the callback passed into the TlsProber instance with a failure
  // response.
  // Note that |receive_stream| and |send_stream|, created on the TCP
  // connection, are only present to fit the callback signature of
  // network::mojom::NetworkContext::CreateTCPConnectedSocketCallback. As error
  // handling has not been set up, the streams should not be used and fall out
  // of scope when this method completes.
  void OnConnectComplete(int result,
                         const std::optional<net::IPEndPoint>& local_addr,
                         const std::optional<net::IPEndPoint>& peer_addr,
                         mojo::ScopedDataPipeConsumerHandle receive_stream,
                         mojo::ScopedDataPipeProducerHandle send_stream);

  // Processes the results of the TLS upgrade.
  // Note that |receive_stream| and |send_stream| are only present to fit the
  // callback signature of
  // network::mojom::NetworkContext::CreateTCPConnectedSocketCallback. As error
  // handling has not been set up, the streams should not be used.
  void OnTlsUpgrade(int result,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream,
                    const std::optional<net::SSLInfo>& ssl_info);

  // Handles disconnects on the TCP connected and TLS client remotes.
  void OnDisconnect();

  // Signals the end of the probe. Manages the clean up and returns a response
  // to the caller.
  void OnDone(int result, ProbeExitEnum probe_exit_enum);

  // Gets the active profile-specific network context.
  const network::NetworkContextGetter network_context_getter_;
  // Contains the hostname and port.
  const net::HostPortPair host_port_pair_;
  // Indicates whether TLS support must be added to the underlying socket.
  const bool negotiate_tls_;
  // Host resolver used for DNS lookup.
  std::unique_ptr<network::SimpleHostResolver> host_resolver_;
  // Holds socket if socket was connected via TCP.
  mojo::Remote<network::mojom::TCPConnectedSocket> tcp_connected_socket_remote_;
  // Holds socket if socket was upgraded to TLS.
  mojo::Remote<network::mojom::TLSClientSocket> tls_client_socket_remote_;
  // Stores the callback invoked once probe is complete or interrupted.
  TlsProbeCompleteCallback callback_;
  // WeakPtr is used when posting tasks to |task_runner_| which might outlive
  // |this|.
  base::WeakPtrFactory<TlsProber> weak_factory_{this};
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_TLS_PROBER_H_
