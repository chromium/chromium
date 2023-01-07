// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_ORIGIN_PROBER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_ORIGIN_PROBER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_probe_result.h"
#include "net/base/address_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class PrefetchProxyCanaryChecker;
class Profile;

// This class handles all probing and canary checks for the prefetch proxy
// feature. Calling code should use |ShouldProbeOrigins| to determine if a probe
// is needed before using prefetched resources and if so, call |Probe|. See
// http://crbug.com/1109992 for more details.
class PrefetchProxyOriginProber {
 public:
  // Allows the url passed to |Probe| to be changed. Only used in testing.
  class ProbeURLOverrideDelegate {
   public:
    virtual GURL OverrideProbeURL(const GURL& url) = 0;
  };

  explicit PrefetchProxyOriginProber(Profile* profile);
  ~PrefetchProxyOriginProber();

  // Run canary checks if they are not already cached.
  void RunCanaryChecksIfNeeded() const;

  // Returns true if a probe needs to be done before using prefetched resources.
  bool ShouldProbeOrigins() const;

  // Sets the probe url override delegate for testing.
  void SetProbeURLOverrideDelegateOverrideForTesting(
      ProbeURLOverrideDelegate* delegate);

  // Tells whether a DNS canary check has completed, either in success or
  // failure. Used for testing.
  bool IsDNSCanaryCheckCompleteForTesting() const;

  // Tells whether a TLS canary check has completed, either in success or
  // failure. Used for testing.
  bool IsTLSCanaryCheckCompleteForTesting() const;

  // Returns the PrefetchProxyCanaryChecker object used for DNS canary checks.
  // Used for testing.
  PrefetchProxyCanaryChecker* GetDNSCanaryCheckerForTesting();

  // Returns the PrefetchProxyCanaryChecker object used for TLS canary checks.
  // Used for testing.
  PrefetchProxyCanaryChecker* GetTLSCanaryCheckerForTesting();

  // Starts a probe to |url| and calls |callback| with an
  // |PrefetchProxyProbeResult| to indicate success.
  using OnProbeResultCallback =
      base::OnceCallback<void(PrefetchProxyProbeResult)>;
  void Probe(const GURL& url, OnProbeResultCallback callback);

 private:
  void DNSProbe(const GURL& url, OnProbeResultCallback callback);
  void HTTPProbe(const GURL& url, OnProbeResultCallback callback);
  void TLSProbe(const GURL& url, OnProbeResultCallback callback);

  // Does a DNS resolution for a DNS or TLS probe, passing all the arguments to
  // |OnDNSResolved|.
  void StartDNSResolution(const GURL& url,
                          OnProbeResultCallback callback,
                          bool also_do_tls_connect);

  // Both DNS and TLS probes need to resolve DNS. This starts the TLS probe with
  // the |addresses| from the DNS resolution.
  void DoTLSProbeAfterDNSResolution(const GURL& url,
                                    OnProbeResultCallback callback,
                                    const net::AddressList& addresses);

  // If the DNS resolution was successful, this will either run |callback| for a
  // DNS probe, or start the TLS socket for a TLS probe. This is determined by
  // |also_do_tls_connect|. If the DNS resolution failed, |callback| is run with
  // failure.
  void OnDNSResolved(
      const GURL& url,
      OnProbeResultCallback callback,
      bool also_do_tls_connect,
      int net_error,
      const absl::optional<net::AddressList>& resolved_addresses);

  // The current profile, not owned.
  raw_ptr<Profile> profile_;

  // Used for testing to change the url passed to |Probe|. Must outlive |this|.
  raw_ptr<ProbeURLOverrideDelegate> override_delegate_ = nullptr;

  // The TLS canary url checker.
  std::unique_ptr<PrefetchProxyCanaryChecker> tls_canary_check_;

  // The DNS canary url checker.
  std::unique_ptr<PrefetchProxyCanaryChecker> dns_canary_check_;

  base::WeakPtrFactory<PrefetchProxyOriginProber> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_ORIGIN_PROBER_H_
