// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_dns_prober.h"

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
    const absl::optional<net::AddressList>& resolved_addresses) {
  if (callback_) {
    std::move(callback_).Run(error, resolved_addresses);
  }
}
