// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_SERVICE_H_
#define CHROME_BROWSER_NET_DNS_PROBE_SERVICE_H_

#include "base/functional/callback.h"
#include "components/error_page/common/net_error_info.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/dns/public/dns_config_overrides.h"

namespace chrome_browser_net {

// Probes the current DNS config servers and Google DNS servers to determine
// the (probable) cause of a recent DNS-related page load error.  Coalesces
// multiple probe requests (perhaps from multiple tabs) and caches the results.
//
// Uses a single DNS attempt per config, and doesn't randomize source ports.
//
// Use DnsProbeServiceFactory to get a service handle.
class DnsProbeService : public KeyedService {
 public:
  using ProbeCallback =
      base::OnceCallback<void(error_page::DnsProbeStatus result)>;

  // Starts a DNS probe, or uses an existing probe result if a probe is already
  // in progress or recently completed. |callback| will be called
  // asynchronously with the probe result.
  virtual void ProbeDns(ProbeCallback callback) = 0;

  // Returns the DnsConfigOverrides being used to mimic the current DNS config.
  virtual net::DnsConfigOverrides GetCurrentConfigOverridesForTesting() = 0;
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_SERVICE_H_
