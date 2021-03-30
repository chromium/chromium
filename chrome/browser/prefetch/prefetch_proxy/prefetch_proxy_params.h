// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PARAMS_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PARAMS_H_

#include <stdint.h>

#include "base/optional.h"
#include "base/time/time.h"
#include "url/gurl.h"

// This command line flag enables NoStatePrefetch on Prefetch Proxy.
extern const char kIsolatedPrerenderEnableNSPCmdLineFlag[];

// Overrides the value returned by
// |PrefetchProxyMaxSubresourcesPerPrerender| when a valid long is given.
extern const char kIsolatedPrerenderLimitNSPSubresourcesCmdLineFlag[];

// Returns true if the Prefetch Proxy feature is enabled.
bool PrefetchProxyIsEnabled();

// The url of the tunnel proxy.
GURL PrefetchProxyProxyHost();

// The header name used to connect to the tunnel proxy.
std::string PrefetchProxyProxyHeaderKey();

// Whether the feature is only enabled for Lite Mode users.
bool PrefetchProxyOnlyForLiteMode();

// Returns true when prefetched pages should run no state prefetch.
bool PrefetchProxyNoStatePrefetchSubresources();

// The maximum number of prefetches that should be done from predictions on a
// Google SRP. nullopt is returned for unlimited. Negative values given by the
// field trial return nullopt.
base::Optional<size_t> PrefetchProxyMaximumNumberOfPrefetches();

// The maximum number of mainframes allowed to be prefetched at the same time.
size_t PrefetchProxyMaximumNumberOfConcurrentPrefetches();

// The maximum number of no state prefetches to attempt, in order to prefetch
// the pages' subresources, while the user is on the SRP. nullopt is returned
// for unlimited. Negative values given by the field trial return nullopt.
base::Optional<size_t> PrefetchProxyMaximumNumberOfNoStatePrefetchAttempts();

// The maximum body length allowed to be prefetched for mainframe responses in
// bytes.
size_t PrefetchProxyMainframeBodyLengthLimit();

// Whether idle sockets should be closed after every prefetch.
bool PrefetchProxyCloseIdleSockets();

// The amount of time to allow before timing out an origin probe.
base::TimeDelta PrefetchProxyProbeTimeout();

// The amount of time to allow a prefetch to take before considering it a
// timeout error.
base::TimeDelta PrefetchProxyTimeoutDuration();

// Whether probing must be done at all.
bool PrefetchProxyProbingEnabled();

// Whether an ISP filtering canary check should be made on browser startup.
bool PrefetchProxyCanaryCheckEnabled();

// The URL to use for the TLS canary check.
GURL PrefetchProxyTLSCanaryCheckURL();

// The URL to use for the DNS canary check.
GURL PrefetchProxyDNSCanaryCheckURL();

// How long a canary check can be cached for the same network.
base::TimeDelta PrefetchProxyCanaryCheckCacheLifetime();

// Experimental control to replace TLS probing with HTTP.
bool PrefetchProxyMustHTTPProbeInsteadOfTLS();

// The maximum number of subresources that will be fetched per prefetched page.
size_t PrefetchProxyMaxSubresourcesPerPrerender();

// Whether a spare renderer should be started after all prefetching and NSP is
// complete.
bool PrefetchProxyStartsSpareRenderer();

// Whether the proxy should decide prefetches based on speculation rules API.
// The default (false) uses Navigation Predictor. When false, prefetch proxy
// can only be used for links from default search to links that are not Google.
// When true, any origin in the origin trial (see
// blink::features::kSpeculationRulesPrefetchProxy) can request a proxied
// prefetch for any cross origin link.
bool PrefetchProxyUseSpeculationRules();

// Whether the given position of a predicted link should be prefetched.
bool PrefetchProxyShouldPrefetchPosition(size_t position);

// The maximum retry-after header value that will be persisted.
base::TimeDelta PrefetchProxyMaxRetryAfterDelta();

// Returns true if an ineligible prefetch request should be put on the network,
// but not cached, to disguise the presence of cookies (or other criteria). The
// return value is randomly decided based on variation params since always
// sending the decoy request is expensive from a data use perspective.
bool PrefetchProxySendDecoyRequestForIneligiblePrefetch();

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PARAMS_H_
