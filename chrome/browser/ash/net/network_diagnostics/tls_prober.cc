// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/tls_prober.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/task_runner.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/cpp/simple_host_resolver.h"

namespace ash::network_diagnostics {

namespace {

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("network_diagnostics_tls",
                                             R"(
      semantics {
        sender: "NetworkDiagnosticsRoutines"
        description:
            "Routines send network traffic to hosts in order to "
            "validate the internet connection on a device."
        trigger:
            "A routine attempts a socket connection or makes an http/s "
            "request."
        data:
            "No data other than the origin (scheme-host-port) is sent. "
            "No user identifier is sent along with the data."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        policy_exception_justification:
            "Not implemented. Does not contain user identifier."
      }
  )");
}

}  // namespace

TlsProber::TlsProber(network::NetworkContextGetter network_context_getter,
                     net::HostPortPair host_port_pair,
                     bool negotiate_tls,
                     TlsProbeCompleteCallback callback)
    : network_context_getter_(std::move(network_context_getter)),
      host_port_pair_(std::move(host_port_pair)),
      negotiate_tls_(negotiate_tls),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  DCHECK(!host_port_pair_.IsEmpty());

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  DCHECK(network_context);

  host_resolver_ = network::SimpleHostResolver::Create(network_context);

  // Resolver host parameter source must be unset or set to ANY in order for DNS
  // queries with BuiltInDnsClientEnabled policy disabled to work (b/353448388).
  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // Unretained(this) is safe here because the callback is invoked directly by
  // |host_resolver_| which is owned by |this|.
  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(host_port_pair_),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      base::BindOnce(&TlsProber::OnHostResolutionComplete,
                     base::Unretained(this)));
}

TlsProber::TlsProber()
    : network_context_getter_(base::NullCallback()), negotiate_tls_(false) {}

TlsProber::~TlsProber() = default;

void TlsProber::OnHostResolutionComplete(
    int result,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  host_resolver_.reset();
  if (result != net::OK) {
    CHECK(!resolved_addresses);
    OnDone(result, ProbeExitEnum::kDnsFailure);
    return;
  }
  CHECK(resolved_addresses);

  network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
      completion_callback = base::BindOnce(&TlsProber::OnConnectComplete,
                                           weak_factory_.GetWeakPtr());
  auto pending_receiver =
      tcp_connected_socket_remote_.BindNewPipeAndPassReceiver();
  // Add a disconnect handler to the TCPConnectedSocket remote.
  tcp_connected_socket_remote_.set_disconnect_handler(
      base::BindOnce(&TlsProber::OnDisconnect, weak_factory_.GetWeakPtr()));

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  CHECK(network_context);

  network_context->CreateTCPConnectedSocket(
      /*local_addr=*/std::nullopt, resolved_addresses.value(),
      /*tcp_connected_socket_options=*/nullptr,
      net::MutableNetworkTrafficAnnotationTag(GetTrafficAnnotationTag()),
      std::move(pending_receiver), /*observer=*/mojo::NullRemote(),
      std::move(completion_callback));
}

void TlsProber::OnConnectComplete(
    int result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(tcp_connected_socket_remote_.is_bound());

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kTcpConnectionFailure);
    return;
  }
  if (!negotiate_tls_) {
    OnDone(result, ProbeExitEnum::kSuccess);
    return;
  }

  DCHECK(peer_addr.has_value());

  auto pending_receiver =
      tls_client_socket_remote_.BindNewPipeAndPassReceiver();
  // Remove the disconnect handler on |tcp_connected_socket_remote_|, which is
  // disconnected from its receiver when it's upgraded to a TLSClientSocket
  // remote.
  tcp_connected_socket_remote_.set_disconnect_handler(base::NullCallback());
  // Add a disconnect handler to the TLSClientSocket remote.
  tls_client_socket_remote_.set_disconnect_handler(
      base::BindOnce(&TlsProber::OnDisconnect, weak_factory_.GetWeakPtr()));
  tcp_connected_socket_remote_->UpgradeToTLS(
      host_port_pair_,
      /*options=*/nullptr,
      net::MutableNetworkTrafficAnnotationTag(GetTrafficAnnotationTag()),
      std::move(pending_receiver),
      /*observer=*/mojo::NullRemote(),
      base::BindOnce(&TlsProber::OnTlsUpgrade, weak_factory_.GetWeakPtr()));
}

void TlsProber::OnTlsUpgrade(int result,
                             mojo::ScopedDataPipeConsumerHandle receive_stream,
                             mojo::ScopedDataPipeProducerHandle send_stream,
                             const std::optional<net::SSLInfo>& ssl_info) {
  // |send_stream| and |receive_stream|, created on the TLS connection, fall out
  // of scope when this method completes.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == net::OK) {
    OnDone(result, ProbeExitEnum::kSuccess);
    return;
  }
  OnDone(result, ProbeExitEnum::kTlsUpgradeFailure);
}

void TlsProber::OnDisconnect() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnDone(net::ERR_FAILED, ProbeExitEnum::kMojoDisconnectFailure);
}

void TlsProber::OnDone(int result, ProbeExitEnum probe_exit_enum) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Invalidate pending callbacks.
  weak_factory_.InvalidateWeakPtrs();
  // Destroy the socket connection.
  tcp_connected_socket_remote_.reset();
  tls_client_socket_remote_.reset();

  std::move(callback_).Run(result, probe_exit_enum);
}

}  // namespace ash::network_diagnostics
