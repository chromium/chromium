// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HOST_RESOLVER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HOST_RESOLVER_H_

#include "base/callback.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace ash {
namespace network_diagnostics {

// Performs a DNS host resolution. This is a single-use class.
class HostResolver : public network::ResolveHostClientBase {
 public:
  struct ResolutionResult {
    ResolutionResult(int result,
                     const net::ResolveErrorInfo& resolve_error_info,
                     const absl::optional<net::AddressList>& resolved_addresses,
                     const absl::optional<net::HostResolverEndpointResults>&
                         endpoint_results_with_metadata);
    ~ResolutionResult();

    int result;
    net::ResolveErrorInfo resolve_error_info;
    absl::optional<net::AddressList> resolved_addresses;
    absl::optional<net::HostResolverEndpointResults>
        endpoint_results_with_metadata;
  };
  using OnResolutionComplete = base::OnceCallback<void(ResolutionResult&)>;

  // Performs the DNS resolution of a specified |host_port_pair|. Note that
  // |callback| will not be called until construction is complete.
  HostResolver(const net::HostPortPair& host_port_pair,
               network::mojom::NetworkContext* network_context,
               OnResolutionComplete callback);

  HostResolver(const HostResolver&) = delete;
  HostResolver& operator=(const HostResolver&) = delete;
  ~HostResolver() override;

  // network::mojom::ResolveHostClient:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

 private:
  // Handles Mojo connection errors during host resolution.
  void OnMojoConnectionError();

  // Callback invoked once resolution is complete.
  OnResolutionComplete callback_;
  // Receiver endpoint to handle host resolution results.
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  // Remote endpoint that calls the network service.
  mojo::Remote<network::mojom::HostResolver> host_resolver_;
};

}  // namespace network_diagnostics
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_HOST_RESOLVER_H_
