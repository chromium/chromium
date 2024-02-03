// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_RUNNER_H_
#define CHROME_BROWSER_NET_DNS_PROBE_RUNNER_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/host_resolver_results.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace chrome_browser_net {

// Runs DNS probes using a HostResolver and evaluates the responses.
// (Currently requests A records for google.com and expects at least one IP
// address in the response.)
// Used by DnsProbeService to probe the system and public DNS configurations.
class DnsProbeRunner : public network::ResolveHostClientBase {
 public:
  static const char kKnownGoodHostname[];

  // Used in histograms; add new entries at the bottom, and don't remove any.
  enum Result {
    UNKNOWN,
    CORRECT,     // Response contains at least one A record.
    INCORRECT,   // Response claimed success but included no A records.
    FAILING,     // Response included an error or was malformed.
    UNREACHABLE  // No response received (timeout, network unreachable, etc.).
  };

  // Creates a probe runner that will use |dns_config_overrides| for the dns
  // configuration and will use |network_context_getter| to get the
  // NetworkContext to create the HostResolver.  The |network_context_getter|
  // may be called multiple times.
  DnsProbeRunner(net::DnsConfigOverrides dns_config_overrides,
                 const network::NetworkContextGetter& network_context_getter);

  DnsProbeRunner(const DnsProbeRunner&) = delete;
  DnsProbeRunner& operator=(const DnsProbeRunner&) = delete;

  ~DnsProbeRunner() override;

  // Starts a probe. |callback| will be called asynchronously when the result
  // is ready, and will not be called if the DnsProbeRunner is destroyed before
  // the probe finishes. Must not be called again until the callback is called,
  // but may be called during the callback.
  void RunProbe(base::OnceClosure callback);

  // Returns true if a probe is running.  Guaranteed to return true after
  // RunProbe returns, and false during and after the callback.
  bool IsRunning() const;

  // Returns the result of the last probe.
  Result result() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return result_;
  }

  // network::ResolveHostClientBase impl:
  void OnComplete(int32_t result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override;

  net::DnsConfigOverrides GetConfigOverridesForTesting() {
    return dns_config_overrides_;
  }

 private:
  void CreateHostResolver();
  void OnMojoConnectionError();

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};

  net::DnsConfigOverrides dns_config_overrides_;
  network::NetworkContextGetter network_context_getter_;

  mojo::Remote<network::mojom::HostResolver> host_resolver_;

  // The callback passed to |RunProbe|.  Cleared right before calling the
  // callback.
  base::OnceClosure callback_;

  Result result_{UNKNOWN};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_RUNNER_H_
