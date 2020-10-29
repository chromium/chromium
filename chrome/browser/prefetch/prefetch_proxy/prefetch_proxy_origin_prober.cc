// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_origin_prober.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/availability/availability_prober.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "url/origin.h"

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

class DNSProber : public network::mojom::ResolveHostClient {
 public:
  using OnDNSResultsCallback = base::OnceCallback<
      void(int, const base::Optional<net::AddressList>& resolved_addresses)>;

  explicit DNSProber(OnDNSResultsCallback callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
  }

  ~DNSProber() override {
    if (callback_) {
      // Indicates some kind of mojo error. Play it safe and return no success.
      std::move(callback_).Run(net::ERR_FAILED, base::nullopt);
    }
  }

  // network::mojom::ResolveHostClient:
  void OnTextResults(const std::vector<std::string>&) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>&) override {}
  void OnComplete(
      int32_t error,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    if (callback_) {
      std::move(callback_).Run(error, resolved_addresses);
    }
  }

 private:
  OnDNSResultsCallback callback_;
};

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
                      const base::Optional<net::IPEndPoint>& local_addr,
                      const base::Optional<net::IPEndPoint>& peer_addr,
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
                      const base::Optional<net::SSLInfo>& ssl_info) {
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

void HTTPProbeHelper(std::unique_ptr<AvailabilityProber> prober,
                     PrefetchProxyOriginProber::OnProbeResultCallback callback,
                     bool success) {
  std::move(callback).Run(success ? PrefetchProxyProbeResult::kTLSProbeSuccess
                                  : PrefetchProxyProbeResult::kTLSProbeFailure);
}

class CanaryCheckDelegate : public AvailabilityProber::Delegate {
 public:
  CanaryCheckDelegate() = default;
  ~CanaryCheckDelegate() = default;

  bool ShouldSendNextProbe() override { return true; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override {
    if (net_error != net::OK)
      return false;
    if (!head)
      return false;
    if (!head->headers)
      return false;
    if (head->headers->response_code() != 200)
      return false;
    if (!body)
      return false;
    // Strip any whitespace, especially trailing newlines.
    return "OK" == base::TrimWhitespaceASCII(*body, base::TRIM_ALL);
  }
};

class OriginProbeDelegate : public AvailabilityProber::Delegate {
 public:
  OriginProbeDelegate() = default;
  ~OriginProbeDelegate() = default;

  bool ShouldSendNextProbe() override { return true; }

  bool IsResponseSuccess(net::Error net_error,
                         const network::mojom::URLResponseHead* head,
                         std::unique_ptr<std::string> body) override {
    return net_error == net::OK;
  }
};

CanaryCheckDelegate* GetCanaryCheckDelegate() {
  static base::NoDestructor<CanaryCheckDelegate> delegate;
  return delegate.get();
}

OriginProbeDelegate* GetOriginProbeDelegate() {
  static base::NoDestructor<OriginProbeDelegate> delegate;
  return delegate.get();
}

// Allows probing to start after a delay so that browser start isn't slowed.
void StartCanaryCheck(base::WeakPtr<AvailabilityProber> canary_checker) {
  // If there is no previously cached result for this network then one should be
  // started. If the previous result is stale, the prober will start a probe
  // during |LastProbeWasSuccessful|.
  if (!canary_checker->LastProbeWasSuccessful().has_value()) {
    canary_checker->SendNowIfInactive(false);
  }
}

}  // namespace

PrefetchProxyOriginProber::PrefetchProxyOriginProber(Profile* profile)
    : profile_(profile) {
  if (!PrefetchProxyProbingEnabled())
    return;
  if (!PrefetchProxyCanaryCheckEnabled())
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("prefetch_proxy_canary_check", R"(
          semantics {
            sender: "Prefetch Proxy Canary Checker"
            description:
              "Sends a request over HTTP to a known host in order to determine "
              "if the network is subject to web filtering. If this request is "
              "blocked, the Prefetch Proxy feature will check that a navigated "
              "site is not blocked by the network before using proxied "
              "resources."
            trigger:
              "Used at browser startup for Lite mode users when the feature is "
              "enabled."
            data: "None."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control Lite mode on Android via the settings menu. "
              "Lite mode is not available on iOS, and on desktop only for "
              "developer testing."
            policy_exception_justification: "Not implemented."
        })");

  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.base_timeout = PrefetchProxyProbeTimeout();
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  tls_canary_check_ = std::make_unique<AvailabilityProber>(
      GetCanaryCheckDelegate(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess(),
      profile_->GetPrefs(),
      AvailabilityProber::ClientName::kIsolatedPrerenderTLSCanaryCheck,
      PrefetchProxyTLSCanaryCheckURL(), AvailabilityProber::HttpMethod::kGet,
      net::HttpRequestHeaders(), retry_policy, timeout_policy,
      traffic_annotation, 10 /* max_cache_entries */,
      PrefetchProxyCanaryCheckCacheLifetime());
  tls_canary_check_->SetOnCompleteCallback(
      base::BindOnce(&PrefetchProxyOriginProber::OnTLSCanaryCheckComplete,
                     weak_factory_.GetWeakPtr()));

  dns_canary_check_ = std::make_unique<AvailabilityProber>(
      GetCanaryCheckDelegate(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess(),
      profile_->GetPrefs(),
      AvailabilityProber::ClientName::kIsolatedPrerenderDNSCanaryCheck,
      PrefetchProxyDNSCanaryCheckURL(), AvailabilityProber::HttpMethod::kGet,
      net::HttpRequestHeaders(), retry_policy, timeout_policy,
      traffic_annotation, 10 /* max_cache_entries */,
      PrefetchProxyCanaryCheckCacheLifetime());

  // This code is running at browser startup. Start the canary check when we get
  // the chance, but there's no point in it being ready for the first navigation
  // since the check won't be done by then anyways.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&StartCanaryCheck, tls_canary_check_->AsWeakPtr()),
          base::TimeDelta::FromSeconds(1));
}

PrefetchProxyOriginProber::~PrefetchProxyOriginProber() = default;

void PrefetchProxyOriginProber::OnTLSCanaryCheckComplete(bool success) {
  // If the TLS check was not successful, don't bother with the DNS check.
  if (!success)
    return;

  StartCanaryCheck(dns_canary_check_->AsWeakPtr());
}

bool PrefetchProxyOriginProber::ShouldProbeOrigins() const {
  if (!PrefetchProxyProbingEnabled()) {
    return false;
  }
  if (!PrefetchProxyCanaryCheckEnabled()) {
    return true;
  }
  DCHECK(tls_canary_check_);
  DCHECK(dns_canary_check_);

  bool tls_success =
      tls_canary_check_->LastProbeWasSuccessful().value_or(false);
  bool dns_success =
      dns_canary_check_->LastProbeWasSuccessful().value_or(false);

  // If both checks have completed and succeeded, then no probing is needed. In
  // every other case, probe.
  return !(tls_success && dns_success);
}

void PrefetchProxyOriginProber::SetProbeURLOverrideDelegateOverrideForTesting(
    ProbeURLOverrideDelegate* delegate) {
  override_delegate_ = delegate;
}

bool PrefetchProxyOriginProber::IsTLSCanaryCheckCompleteForTesting() const {
  return tls_canary_check_->LastProbeWasSuccessful().has_value();
}

bool PrefetchProxyOriginProber::IsDNSCanaryCheckActiveForTesting() const {
  return dns_canary_check_->is_active();
}

void PrefetchProxyOriginProber::Probe(const GURL& url,
                                      OnProbeResultCallback callback) {
  DCHECK(ShouldProbeOrigins());

  GURL probe_url = url;
  if (override_delegate_) {
    probe_url = override_delegate_->OverrideProbeURL(probe_url);
  }

  bool tls_canary_check_success =
      tls_canary_check_
          ? tls_canary_check_->LastProbeWasSuccessful().value_or(false)
          : false;

  if (!tls_canary_check_success) {
    if (PrefetchProxyMustHTTPProbeInsteadOfTLS()) {
      HTTPProbe(probe_url, std::move(callback));
      return;
    }
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
  net::NetworkIsolationKey nik =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url))
          .network_isolation_key();

  network::mojom::ResolveHostParametersPtr resolve_host_parameters =
      network::mojom::ResolveHostParameters::New();
  // This action is navigation-blocking, so use the highest priority.
  resolve_host_parameters->initial_priority = net::RequestPriority::HIGHEST;

  mojo::PendingRemote<network::mojom::ResolveHostClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DNSProber>(base::BindOnce(
          &PrefetchProxyOriginProber::OnDNSResolved, weak_factory_.GetWeakPtr(),
          url, std::move(callback), also_do_tls_connect)),
      client_remote.InitWithNewPipeAndPassReceiver());

  content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetNetworkContext()
      ->ResolveHost(net::HostPortPair::FromURL(url), nik,
                    std::move(resolve_host_parameters),
                    std::move(client_remote));
}

void PrefetchProxyOriginProber::HTTPProbe(const GURL& url,
                                          OnProbeResultCallback callback) {
  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.base_timeout = PrefetchProxyProbeTimeout();
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  std::unique_ptr<AvailabilityProber> prober =
      std::make_unique<AvailabilityProber>(
          GetOriginProbeDelegate(),
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetURLLoaderFactoryForBrowserProcess(),
          nullptr /* pref_service */,
          AvailabilityProber::ClientName::kIsolatedPrerenderOriginCheck, url,
          AvailabilityProber::HttpMethod::kHead, net::HttpRequestHeaders(),
          retry_policy, timeout_policy, GetProbingTrafficAnnotation(),
          0 /* max_cache_entries */,
          base::TimeDelta::FromSeconds(0) /* revalidate_cache_after */);
  AvailabilityProber* prober_ptr = prober.get();

  // Transfer ownership of the prober to the callback so that the class instance
  // is automatically destroyed when the probe is complete.
  auto owning_callback =
      base::BindOnce(&HTTPProbeHelper, std::move(prober), std::move(callback));
  prober_ptr->SetOnCompleteCallback(base::BindOnce(std::move(owning_callback)));

  prober_ptr->SendNowIfInactive(false /* send_only_in_foreground */);
}

void PrefetchProxyOriginProber::OnDNSResolved(
    const GURL& url,
    OnProbeResultCallback callback,
    bool also_do_tls_connect,
    int net_error,
    const base::Optional<net::AddressList>& resolved_addresses) {
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

  content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetNetworkContext()
      ->CreateTCPConnectedSocket(
          /*local_addr=*/base::nullopt, addresses,
          /*tcp_connected_socket_options=*/nullptr,
          net::MutableNetworkTrafficAnnotationTag(
              GetProbingTrafficAnnotation()),
          prober->GetTCPSocketReceiver(),
          /*observer=*/mojo::NullRemote(), prober->GetOnTCPConnectedCallback());

  // |prober| manages its own lifetime, using the mojo pipes.
  prober.release();
}
