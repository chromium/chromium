// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/tls_prober.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resolve_host_client_base.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

const char kTrafficAnnotation[] = R"(
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
          }
      )";

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("network_diagnostics_routines",
                                             kTrafficAnnotation);
}

}  // namespace

class TlsProber::HostResolver : public network::ResolveHostClientBase {
 public:
  HostResolver(network::mojom::NetworkContext* network_context,
               TlsProber* tls_prober);
  HostResolver(const HostResolver&) = delete;
  HostResolver& operator=(const HostResolver&) = delete;
  ~HostResolver() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override;

  // Performs the DNS resolution.
  void Run(const GURL& url);

  network::mojom::NetworkContext* network_context() const {
    return network_context_;
  }

 private:
  void CreateHostResolver();
  void OnMojoConnectionError();

  network::mojom::NetworkContext* network_context_ = nullptr;  // Unowned
  TlsProber* tls_prober_;                                      // Unowned
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
};

TlsProber::HostResolver::HostResolver(
    network::mojom::NetworkContext* network_context,
    TlsProber* tls_prober)
    : network_context_(network_context), tls_prober_(tls_prober) {
  DCHECK(network_context_);
  DCHECK(tls_prober_);
}

TlsProber::HostResolver::~HostResolver() = default;

void TlsProber::HostResolver::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  receiver_.reset();
  host_resolver_.reset();

  tls_prober_->OnHostResolutionComplete(result, resolve_error_info,
                                        resolved_addresses);
}

void TlsProber::HostResolver::Run(const GURL& url) {
  CreateHostResolver();
  DCHECK(host_resolver_);
  DCHECK(!receiver_.is_bound());

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  host_resolver_->ResolveHost(net::HostPortPair::FromURL(url),
                              net::NetworkIsolationKey::CreateTransient(),
                              std::move(parameters),
                              receiver_.BindNewPipeAndPassRemote());
}

void TlsProber::HostResolver::CreateHostResolver() {
  network_context()->CreateHostResolver(
      net::DnsConfigOverrides(), host_resolver_.BindNewPipeAndPassReceiver());
  // Disconnect handler will be invoked if the network service crashes.
  host_resolver_.set_disconnect_handler(base::BindOnce(
      &HostResolver::OnMojoConnectionError, base::Unretained(this)));
}

void TlsProber::HostResolver::OnMojoConnectionError() {
  OnComplete(net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
             base::nullopt);
}

TlsProber::TlsProber(NetworkContextGetter network_context_getter,
                     const GURL& url,
                     TlsProbeCompleteCallback callback)
    : network_context_getter_(std::move(network_context_getter)),
      url_(url),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback_);
  DCHECK(url_.is_valid());

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  DCHECK(network_context);

  host_resolver_ = std::make_unique<HostResolver>(network_context, this);
  DCHECK(host_resolver_);
  host_resolver_->Run(url);
}

TlsProber::TlsProber() = default;

TlsProber::~TlsProber() = default;

void TlsProber::OnHostResolutionComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool success = result == net::OK && !resolved_addresses->empty() &&
                 resolved_addresses.has_value();
  if (!success) {
    OnDone(result, ProbeExitEnum::kDnsFailure);
    return;
  }

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
  DCHECK(network_context);

  network_context->CreateTCPConnectedSocket(
      /*local_addr=*/base::nullopt, resolved_addresses.value(),
      /*options=*/nullptr,
      net::MutableNetworkTrafficAnnotationTag(GetTrafficAnnotationTag()),
      std::move(pending_receiver), /*observer=*/mojo::NullRemote(),
      std::move(completion_callback));
}

void TlsProber::OnConnectComplete(
    int result,
    const base::Optional<net::IPEndPoint>& local_addr,
    const base::Optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(tcp_connected_socket_remote_.is_bound());

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kTcpConnectionFailure);
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
      net::HostPortPair::FromURL(url_),
      /*options=*/nullptr,
      net::MutableNetworkTrafficAnnotationTag(GetTrafficAnnotationTag()),
      std::move(pending_receiver),
      /*observer=*/mojo::NullRemote(),
      base::BindOnce(&TlsProber::OnTlsUpgrade, weak_factory_.GetWeakPtr()));
}

void TlsProber::OnTlsUpgrade(int result,
                             mojo::ScopedDataPipeConsumerHandle receive_stream,
                             mojo::ScopedDataPipeProducerHandle send_stream,
                             const base::Optional<net::SSLInfo>& ssl_info) {
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

}  // namespace network_diagnostics
}  // namespace chromeos
