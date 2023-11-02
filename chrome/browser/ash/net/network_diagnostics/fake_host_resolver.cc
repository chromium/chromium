// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/fake_host_resolver.h"

#include <cstdint>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace network_diagnostics {

FakeHostResolver::DnsResult::DnsResult(
    int32_t result,
    net::ResolveErrorInfo resolve_error_info,
    absl::optional<net::AddressList> resolved_addresses,
    absl::optional<net::HostResolverEndpointResults>
        endpoint_results_with_metadata)
    : result_(result),
      resolve_error_info_(resolve_error_info),
      resolved_addresses_(resolved_addresses),
      endpoint_results_with_metadata_(endpoint_results_with_metadata) {}

FakeHostResolver::DnsResult::~DnsResult() = default;

FakeHostResolver::FakeHostResolver(
    mojo::PendingReceiver<network::mojom::HostResolver> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeHostResolver::~FakeHostResolver() = default;

void FakeHostResolver::ResolveHost(
    network::mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient>
        pending_response_client) {
  if (disconnect_) {
    receiver_.reset();
    return;
  }
  response_client_.Bind(std::move(pending_response_client));

  DCHECK(fake_dns_result_);
  response_client_->OnComplete(
      fake_dns_result_->result_, fake_dns_result_->resolve_error_info_,
      fake_dns_result_->resolved_addresses_,
      fake_dns_result_->endpoint_results_with_metadata_);
  fake_dns_result_.reset();
}

void FakeHostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
    MdnsListenCallback callback) {
  NOTIMPLEMENTED();
}

void FakeHostResolver::SetFakeDnsResult(
    std::unique_ptr<DnsResult> fake_dns_result) {
  DCHECK(!fake_dns_result_);

  fake_dns_result_ = std::move(fake_dns_result);
}

}  // namespace network_diagnostics
}  // namespace ash
