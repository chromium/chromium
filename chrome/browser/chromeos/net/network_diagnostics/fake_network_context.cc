// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/fake_network_context.h"

#include <utility>

namespace chromeos {
namespace network_diagnostics {

FakeNetworkContext::FakeNetworkContext() = default;

FakeNetworkContext::~FakeNetworkContext() = default;

void FakeNetworkContext::CreateHostResolver(
    const base::Optional<net::DnsConfigOverrides>& config_overrides,
    mojo::PendingReceiver<network::mojom::HostResolver> receiver) {
  DCHECK(!resolver_);
  resolver_ = std::make_unique<FakeHostResolver>(std::move(receiver));
  resolver_->set_fake_dns_results(std::move(fake_dns_results_));
}

}  // namespace network_diagnostics
}  // namespace chromeos
