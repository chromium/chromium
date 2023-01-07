// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_canary_checker.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_dns_prober.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace {

net::NetworkTrafficAnnotationTag GetProbingTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("prefetch_proxy_probe", R"(
        semantics {
          sender: "Prefetch Proxy Probe Loader"
          description:
            "Verifies the end to end connection between Chrome and the "
            "origin site that the user is currently navigating to. This is "
            "done during a navigation that was previously prerendered over a "
            "proxy to check that the site is not blocked by middleboxes. "
            "Such prerenders will be used to prefetch render-blocking "
            "content before being navigated by the user without impacting "
            "privacy."
          trigger:
            "Used for sites off of Google SRPs (Search Result Pages) only "
            "for Lite mode users when the experimental feature flag is "
            "enabled."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via the settings menu. "
            "Lite mode is not available on iOS, and on desktop only for "
            "developer testing."
          policy_exception_justification: "Not implemented."
      })");
}

void TLSDropHandler(base::OnceClosure ui_only_callback) {
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(ui_only_callback));
}

class TLSProber {
 public:
  TLSProber(const GURL& url,
            PrefetchProxyOriginProber::OnProbeResultCallback callback)
      : url_(url), callback_(std::move(callback)) {}
  ~TLSProber() { DCHECK(!callback_); }

  network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
  GetOnTCPConnectedCallback() {
    network::mojom::NetworkContext::CreateTCPConnectedSocketCallback
        tcp_handler = base::BindOnce(&TLSProber::OnTCPConnected,
                                     weak_factory_.GetWeakPtr());

    return mojo::WrapCallbackWithDropHandler(std::move(tcp_handler),
                                             GetDropHandler());
  }

  mojo::PendingReceiver<network::mojom::TCPConnectedSocket>
  GetTCPSocketReceiver() {
    return tcp_socket_.BindNewPipeAndPassReceiver();
  }

 private:
  void OnTCPConnected(int result,
                      const absl::optional<net::IPEndPoint>& local_addr,
                      const absl::optional<net::IPEndPoint>& peer_addr,
                      mojo::ScopedDataPipeConsumerHandle receive_stream,
                      mojo::ScopedDataPipeProducerHandle send_stream) {
    if (result != net::OK) {
      HandleFailure();
      return;
    }

    network::mojom::TCPConnectedSocket::UpgradeToTLSCallback tls_handler =
        base::BindOnce(&TLSProber::OnUpgradeToTLS, weak_factory_.GetWeakPtr());

    tcp_socket_->UpgradeToTLS(
        net::HostPortPair::FromURL(url_), /*options=*/nullptr,
        net::MutableNetworkTrafficAnnotationTag(GetProbingTrafficAnnotation()),
        tls_socket_.BindNewPipeAndPassReceiver(),
        /*observer=*/mojo::NullRemote(),
        mojo::WrapCallbackWithDropHandler(std::move(tls_handler),
                                          GetDropHandler()));
  }

  void OnUpgradeToTLS(int result,
                      mojo::ScopedDataPipeConsumerHandle receive_stream,
                      mojo::ScopedDataPipeProducerHandle send_stream,
                      const absl::optional<net::SSLInfo>& ssl_info) {
    std::move(callback_).Run(result == net::OK
                                 ? PrefetchProxyProbeResult::kTLSProbeSuccess
                                 : PrefetchProxyProbeResult::kTLSProbeFailure);
    delete this;
  }

  base::OnceClosure GetDropHandler() {
    // The drop handler is not guaranteed to be run on the original thread. Use
    // the anon method above to fix that.
    return base::BindOnce(
        &TLSDropHandler,
        base::BindOnce(&TLSProber::HandleFailure, weak_factory_.GetWeakPtr()));
  }

  void HandleFailure() {
    std::move(callback_).Run(PrefetchProxyProbeResult::kTLSProbeFailure);
    delete this;
  }

  // The URL of the resource being probed. Only the host:port is used.
  const GURL url_;

  // The callback to run when the probe is complete.
  PrefetchProxyOriginProber::OnProbeResultCallback callback_;

  // Mojo sockets. We only test that both can be connected.
  mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_;
  mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;

  base::WeakPtrFactory<TLSProber> weak_factory_{this};
};

}  // namespace

PrefetchProxyOriginProber::PrefetchProxyOriginProber(Profile* profile)
    : profile_(profile) {
  if (!PrefetchProxyProbingEnabled())
    return;
  if (!PrefetchProxyCanaryCheckEnabled())
    return;

  PrefetchProxyCanaryChecker::RetryPolicy retry_policy;
  retry_policy.max_retries = PrefetchProxyCanaryCheckRetries();

  if (PrefetchProxyTLSCanaryCheckEnabled()) {
    tls_canary_check_ = std::make_unique<PrefetchProxyCanaryChecker>(
        profile_, PrefetchProxyCanaryChecker::CheckType::kTLS,
        PrefetchProxyTLSCanaryCheckURL(), retry_policy,
        PrefetchProxyCanaryCheckTimeout(),
        PrefetchProxyCanaryCheckCacheLifetime());
  }

  dns_canary_check_ = std::make_unique<PrefetchProxyCanaryChecker>(
      profile_, PrefetchProxyCanaryChecker::CheckType::kDNS,
      PrefetchProxyDNSCanaryCheckURL(), retry_policy,
      PrefetchProxyCanaryCheckTimeout(),
      PrefetchProxyCanaryCheckCacheLifetime());
}

PrefetchProxyOriginProber::~PrefetchProxyOriginProber() = default;

void PrefetchProxyOriginProber::RunCanaryChecksIfNeeded() const {
  if (!PrefetchProxyProbingEnabled() || !PrefetchProxyCanaryCheckEnabled()) {
    return;
  }
  dns_canary_check_->RunChecksIfNeeded();
  if (tls_canary_check_) {
    tls_canary_check_->RunChecksIfNeeded();
  }
}

bool PrefetchProxyOriginProber::ShouldProbeOrigins() const {
  if (!PrefetchProxyProbingEnabled()) {
    return false;
  }
  if (!PrefetchProxyCanaryCheckEnabled()) {
    return true;
  }

  // We call LastProbeWasSuccessful on all enabled canary checks to make sure
  // their cache gets refreshed if necessary.
  DCHECK(dns_canary_check_);
  bool dns_success = dns_canary_check_->CanaryCheckSuccessful().value_or(false);
  bool tls_success = true;
  if (tls_canary_check_) {
    tls_success = tls_canary_check_->CanaryCheckSuccessful().value_or(false);
  }

  // If either check has failed or not completed in time, probe.
  return !dns_success || !tls_success;
}

void PrefetchProxyOriginProber::SetProbeURLOverrideDelegateOverrideForTesting(
    ProbeURLOverrideDelegate* delegate) {
  override_delegate_ = delegate;
}

bool PrefetchProxyOriginProber::IsDNSCanaryCheckCompleteForTesting() const {
  return dns_canary_check_->CanaryCheckSuccessful().has_value();
}

bool PrefetchProxyOriginProber::IsTLSCanaryCheckCompleteForTesting() const {
  return tls_canary_check_->CanaryCheckSuccessful().has_value();
}

void PrefetchProxyOriginProber::Probe(const GURL& url,
                                      OnProbeResultCallback callback) {
  GURL probe_url = url;
  if (override_delegate_) {
    probe_url = override_delegate_->OverrideProbeURL(probe_url);
  }

  // If canary checks are disabled, or if the TLS canary check is enabled and
  // failed (or did not complete), do TLS probing.
  if (!PrefetchProxyCanaryCheckEnabled() ||
      (tls_canary_check_ &&
       !tls_canary_check_->CanaryCheckSuccessful().value_or(false))) {
    TLSProbe(probe_url, std::move(callback));
    return;
  }

  DNSProbe(probe_url, std::move(callback));
}

void PrefetchProxyOriginProber::DNSProbe(const GURL& url,
                                         OnProbeResultCallback callback) {
  StartDNSResolution(url, std::move(callback), /*also_do_tls_connect=*/false);
}

void PrefetchProxyOriginProber::TLSProbe(const GURL& url,
                                         OnProbeResultCallback callback) {
  StartDNSResolution(url, std::move(callback), /*also_do_tls_connect=*/true);
}

void PrefetchProxyOriginProber::StartDNSResolution(
    const GURL& url,
    OnProbeResultCallback callback,
    bool also_do_tls_connect) {
  net::NetworkAnonymizationKey nak =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url))
          .network_anonymization_key();

  network::mojom::ResolveHostParametersPtr resolve_host_parameters =
      network::mojom::ResolveHostParameters::New();
  // This action is navigation-blocking, so use the highest priority.
  resolve_host_parameters->initial_priority = net::RequestPriority::HIGHEST;

  mojo::PendingRemote<network::mojom::ResolveHostClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrefetchProxyDNSProber>(base::BindOnce(
          &PrefetchProxyOriginProber::OnDNSResolved, weak_factory_.GetWeakPtr(),
          url, std::move(callback), also_do_tls_connect)),
      client_remote.InitWithNewPipeAndPassReceiver());

  // TODO(crbug.com/1355169): Consider passing a SchemeHostPort to trigger HTTPS
  // DNS resource record query.
  profile_->GetDefaultStoragePartition()->GetNetworkContext()->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair::FromURL(url)),
      nak, std::move(resolve_host_parameters), std::move(client_remote));
}

void PrefetchProxyOriginProber::OnDNSResolved(
    const GURL& url,
    OnProbeResultCallback callback,
    bool also_do_tls_connect,
    int net_error,
    const absl::optional<net::AddressList>& resolved_addresses) {
  bool successful = net_error == net::OK && resolved_addresses &&
                    !resolved_addresses->empty();

  // A TLS connection needs the resolved addresses, so it also fails here.
  if (!successful) {
    std::move(callback).Run(PrefetchProxyProbeResult::kDNSProbeFailure);
    return;
  }

  if (!also_do_tls_connect) {
    std::move(callback).Run(PrefetchProxyProbeResult::kDNSProbeSuccess);
    return;
  }

  DoTLSProbeAfterDNSResolution(url, std::move(callback), *resolved_addresses);
}

void PrefetchProxyOriginProber::DoTLSProbeAfterDNSResolution(
    const GURL& url,
    OnProbeResultCallback callback,
    const net::AddressList& addresses) {
  DCHECK(!addresses.empty());

  std::unique_ptr<TLSProber> prober =
      std::make_unique<TLSProber>(url, std::move(callback));

  profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateTCPConnectedSocket(
          /*local_addr=*/absl::nullopt, addresses,
          /*tcp_connected_socket_options=*/nullptr,
          net::MutableNetworkTrafficAnnotationTag(
              GetProbingTrafficAnnotation()),
          prober->GetTCPSocketReceiver(),
          /*observer=*/mojo::NullRemote(), prober->GetOnTCPConnectedCallback());

  // |prober| manages its own lifetime, using the mojo pipes.
  prober.release();
}

PrefetchProxyCanaryChecker*
PrefetchProxyOriginProber::GetDNSCanaryCheckerForTesting() {
  return dns_canary_check_.get();
}

PrefetchProxyCanaryChecker*
PrefetchProxyOriginProber::GetTLSCanaryCheckerForTesting() {
  return tls_canary_check_.get();
}
