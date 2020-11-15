// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_HOST_RESOLVER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_HOST_RESOLVER_H_

#include <memory>
#include <utility>

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

  // Sets the fake DNS result for single host resolutions.
  void SetFakeDnsResult(std::unique_ptr<DnsResult> fake_dns_result);

  // If set to true, the binding pipe will be disconnected when attempting to
  // connect.
  void set_disconnect_during_host_resolution(bool disconnect) {
    disconnect_ = disconnect;
  }

 private:
  // Handles calls to the HostResolver.
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  // Responds to calls made to |this|.
  mojo::Remote<network::mojom::ResolveHostClient> response_client_;
  // Use the |fake_dns_result| to fake a single host resolution.
  std::unique_ptr<DnsResult> fake_dns_result_ = nullptr;
  // Used to mimic the scenario where network::mojom::HostResolver receiver
  // is disconnected.
  bool disconnect_ = false;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_HOST_RESOLVER_H_
