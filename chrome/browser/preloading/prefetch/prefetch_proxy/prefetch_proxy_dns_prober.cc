// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_dns_prober.h"

PrefetchProxyDNSProber::PrefetchProxyDNSProber(OnDNSResultsCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

PrefetchProxyDNSProber::~PrefetchProxyDNSProber() {
  if (callback_) {
    // Indicates some kind of mojo error. Play it safe and return no success.
    std::move(callback_).Run(net::ERR_FAILED, absl::nullopt);
  }
}

void PrefetchProxyDNSProber::OnComplete(
    int32_t error,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses,
    const absl::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  if (callback_) {
    std::move(callback_).Run(error, resolved_addresses);
  }
}
