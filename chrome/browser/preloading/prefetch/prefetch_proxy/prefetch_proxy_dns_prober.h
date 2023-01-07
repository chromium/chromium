// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_DNS_PROBER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_DNS_PROBER_H_

#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// PrefetchProxyDNSProber is a simple ResolveHostClient implementation that
// performs DNS resolution and invokes a callback upon completion.
class PrefetchProxyDNSProber : public network::mojom::ResolveHostClient {
 public:
  using OnDNSResultsCallback = base::OnceCallback<
      void(int, const absl::optional<net::AddressList>& resolved_addresses)>;

  explicit PrefetchProxyDNSProber(OnDNSResultsCallback callback);
  ~PrefetchProxyDNSProber() override;

  // network::mojom::ResolveHostClient:
  void OnTextResults(const std::vector<std::string>&) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>&) override {}
  void OnComplete(int32_t error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

 private:
  OnDNSResultsCallback callback_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_DNS_PROBER_H_
