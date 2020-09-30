// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_HOST_RESOLVER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_HOST_RESOLVER_H_

#include <deque>

#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace chromeos {
namespace network_diagnostics {

// Used in unit tests, the FakeHostResolver class simulates the behavior of a
// host resolver.
class FakeHostResolver : public network::mojom::HostResolver {
 public:
  struct DnsResult {
   public:
    DnsResult(int32_t result,
              net::ResolveErrorInfo resolve_error_info,
              base::Optional<net::AddressList> resolved_addresses);
    ~DnsResult();

    int result_;
    net::ResolveErrorInfo resolve_error_info_;
    base::Optional<net::AddressList> resolved_addresses_;
  };

  FakeHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> receiver);
  ~FakeHostResolver() override;

  // network::mojom::HostResolver
  void ResolveHost(const net::HostPortPair& host,
                   const net::NetworkIsolationKey& network_isolation_key,
                   network::mojom::ResolveHostParametersPtr optional_parameters,
                   mojo::PendingRemote<network::mojom::ResolveHostClient>
                       pending_response_client) override;
  void MdnsListen(
      const net::HostPortPair& host,
      net::DnsQueryType query_type,
      mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override;

  // Sets the fake dns results.
  void set_fake_dns_results(std::deque<DnsResult*> fake_dns_results) {
    fake_dns_results_ = std::move(fake_dns_results);
  }

 private:
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  // Use the list of fake dns results to fake different responses for multiple
  // calls to the host_resolver's ResolveHost().
  std::deque<DnsResult*> fake_dns_results_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_HOST_RESOLVER_H_
