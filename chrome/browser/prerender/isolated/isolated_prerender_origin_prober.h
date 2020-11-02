// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_ORIGIN_PROBER_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_ORIGIN_PROBER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_probe_result.h"
#include "net/base/address_list.h"
#include "url/gurl.h"

class AvailabilityProber;
class Profile;

// This class handles all probing and canary checks for the isolated prerender
// feature. Calling code should use |ShouldProbeOrigins| to determine if a probe
// is needed before using prefetched resources and if so, call |Probe|. See
// http://crbug.com/1109992 for more details.
class IsolatedPrerenderOriginProber {
 public:
  // Allows the url passed to |Probe| to be changed. Only used in testing.
  class ProbeURLOverrideDelegate {
   public:
    virtual GURL OverrideProbeURL(const GURL& url) = 0;
  };

  explicit IsolatedPrerenderOriginProber(Profile* profile);
  ~IsolatedPrerenderOriginProber();

  // Returns true if a probe needs to be done before using prefetched resources.
  bool ShouldProbeOrigins() const;

  // Sets the probe url override delegate for testing.
  void SetProbeURLOverrideDelegateOverrideForTesting(
      ProbeURLOverrideDelegate* delegate);

  // Tells whether a TLS canary check has completed, either in success or
  // failure. Used for testing.
  bool IsTLSCanaryCheckCompleteForTesting() const;

  // Tells whether a DNS canary check is active. Used for testing.
  bool IsDNSCanaryCheckActiveForTesting() const;

  // Starts a probe to |url| and calls |callback| with an
  // |IsolatedPrerenderProbeResult| to indicate success.
  using OnProbeResultCallback =
      base::OnceCallback<void(IsolatedPrerenderProbeResult)>;
  void Probe(const GURL& url, OnProbeResultCallback callback);

 private:
  void OnTLSCanaryCheckComplete(bool success);

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
      const base::Optional<net::AddressList>& resolved_addresses);

  // The current profile, not owned.
  Profile* profile_;

  // Used for testing to change the url passed to |Probe|. Must outlive |this|.
  ProbeURLOverrideDelegate* override_delegate_ = nullptr;

  // The TLS canary url checker.
  std::unique_ptr<AvailabilityProber> tls_canary_check_;

  // The DNS canary url checker.
  std::unique_ptr<AvailabilityProber> dns_canary_check_;

  base::WeakPtrFactory<IsolatedPrerenderOriginProber> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_ORIGIN_PROBER_H_
