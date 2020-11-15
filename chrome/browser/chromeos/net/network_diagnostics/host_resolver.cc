// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/dns_config_overrides.h"

namespace chromeos {
namespace network_diagnostics {

HostResolver::ResolutionResult::ResolutionResult(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses)
    : result(result),
      resolve_error_info(resolve_error_info),
      resolved_addresses(resolved_addresses) {}

HostResolver::ResolutionResult::~ResolutionResult() = default;

HostResolver::HostResolver(const net::HostPortPair& host_port_pair,
                           network::mojom::NetworkContext* network_context,
                           OnResolutionComplete callback)
    : callback_(std::move(callback)) {
  DCHECK(network_context);
  DCHECK(callback_);

  network_context->CreateHostResolver(
      net::DnsConfigOverrides(), host_resolver_.BindNewPipeAndPassReceiver());
  // Disconnect handler will be invoked if the network service crashes.
  host_resolver_.set_disconnect_handler(base::BindOnce(
      &HostResolver::OnMojoConnectionError, base::Unretained(this)));

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->source = net::HostResolverSource::DNS;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  host_resolver_->ResolveHost(
      host_port_pair, net::NetworkIsolationKey::CreateTransient(),
      std::move(parameters), receiver_.BindNewPipeAndPassRemote());
}

HostResolver::~HostResolver() = default;

void HostResolver::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  receiver_.reset();
  host_resolver_.reset();

  ResolutionResult resolution_result{result, resolve_error_info,
                                     resolved_addresses};
  std::move(callback_).Run(resolution_result);
}

void HostResolver::OnMojoConnectionError() {
  OnComplete(net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
             base::nullopt);
}

}  // namespace network_diagnostics
}  // namespace chromeos
