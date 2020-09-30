// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/fake_host_resolver.h"

#include <cstdint>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/dns/public/resolve_error_info.h"

namespace chromeos {
namespace network_diagnostics {

FakeHostResolver::DnsResult::DnsResult(
    int32_t result,
    net::ResolveErrorInfo resolve_error_info,
    base::Optional<net::AddressList> resolved_addresses)
    : result_(result),
      resolve_error_info_(resolve_error_info),
      resolved_addresses_(resolved_addresses) {}

FakeHostResolver::DnsResult::~DnsResult() = default;

FakeHostResolver::FakeHostResolver(
    mojo::PendingReceiver<network::mojom::HostResolver> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeHostResolver::~FakeHostResolver() = default;

void FakeHostResolver::ResolveHost(
    const net::HostPortPair& host,
    const net::NetworkIsolationKey& network_isolation_key,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient>
        pending_response_client) {
  mojo::Remote<network::mojom::ResolveHostClient> response_client(
      std::move(pending_response_client));
  DnsResult* result = fake_dns_results_.front();
  DCHECK(result);
  fake_dns_results_.pop_front();
  response_client->OnComplete(result->result_, result->resolve_error_info_,
                              result->resolved_addresses_);
}

void FakeHostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
    MdnsListenCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace network_diagnostics
}  // namespace chromeos
