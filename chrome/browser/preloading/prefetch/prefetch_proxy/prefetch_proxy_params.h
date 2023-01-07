// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PARAMS_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PARAMS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class PrefService;

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

// Returns true when prefetched pages should run no state prefetch.
bool PrefetchProxyNoStatePrefetchSubresources();

// The maximum number of prefetches that should be done from predictions on a
// Google SRP. nullopt is returned for unlimited. Negative values given by the
// field trial return nullopt.
absl::optional<size_t> PrefetchProxyMaximumNumberOfPrefetches();

// The maximum number of mainframes allowed to be prefetched at the same time.
size_t PrefetchProxyMaximumNumberOfConcurrentPrefetches();

// The maximum number of no state prefetches to attempt, in order to prefetch
// the pages' subresources, while the user is on the SRP. nullopt is returned
// for unlimited. Negative values given by the field trial return nullopt.
absl::optional<size_t> PrefetchProxyMaximumNumberOfNoStatePrefetchAttempts();

// The maximum body length allowed to be prefetched for mainframe responses in
// bytes.
size_t PrefetchProxyMainframeBodyLengthLimit();

// Whether idle sockets should be closed after every prefetch.
bool PrefetchProxyCloseIdleSockets();

// The amount of time to allow before timing out a canary check.
base::TimeDelta PrefetchProxyCanaryCheckTimeout();

// The number of retries to allow for canary checks.
int PrefetchProxyCanaryCheckRetries();

// The amount of time to allow a prefetch to take before considering it a
// timeout error.
base::TimeDelta PrefetchProxyTimeoutDuration();

// Whether probing must be done at all.
bool PrefetchProxyProbingEnabled();

// Whether an ISP filtering canary check should be made on browser startup.
bool PrefetchProxyCanaryCheckEnabled();

// Whether the TLS ISP filtering canary check should enabled. Only has effect if
// canary checks are enabled (PrefetchProxyCanaryCheckEnabled is true). When
// false, only the DNS canary check will be performed. When true, both the DNS
// and TLS canary checks will be enabled.
bool PrefetchProxyTLSCanaryCheckEnabled();

// The URL to use for the TLS canary check.
GURL PrefetchProxyTLSCanaryCheckURL();

// The URL to use for the DNS canary check.
GURL PrefetchProxyDNSCanaryCheckURL();

// How long a canary check can be cached for the same network.
base::TimeDelta PrefetchProxyCanaryCheckCacheLifetime();

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
// sending the decoy request is expensive from a data use perspective. Decoys
// may be disabled for users that opted-in to "Make Search and Browsing Better".
bool PrefetchProxySendDecoyRequestForIneligiblePrefetch(
    PrefService* pref_service);

// Returns true if any domain can issue private prefetches using the Google
// proxy. Normally, this is restricted to Google domains.
bool PrefetchProxyAllowAllDomains();

// Returns true if any domain can issue private prefetches using the Google
// proxy, so long as the user opted-in to extended preloading. This allows us
// to disable the prefetch proxy on non-Google origins via Finch.
bool PrefetchProxyAllowAllDomainsForExtendedPreloading();

// The maximum time a prefetched response is servable.
base::TimeDelta PrefetchProxyCacheableDuration();

// This value is included in the |PrefetchProxyProxyHeaderKey| request header.
// The tunnel proxy will use this to determine what, if any, experimental
// behavior to apply to requests. If the client is not in any server experiment
// group, this will return an empty string.
std::string PrefetchProxyServerExperimentGroup();

// Whether each prefetch should have its own isolated network context (return
// true), or if all prefetches from a main frame should share a single isolated
// network context (returns false).
bool PrefetchProxyUseIndividualNetworkContextsForEachPrefetch();

// Whether the PrefetchProxy code can handle non-private prefetches.
bool PrefetchProxySupportNonPrivatePrefetches();

// Retrieves a host for which the prefetch proxy should be bypassed for testing
// purposes.
absl::optional<std::string> PrefetchProxyBypassProxyForHost();

// Whether only prefetched resources with a text/html MIME type should be used.
// If this is false, there is no MIME type restriction.
bool PrefetchProxyHTMLOnly();

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PARAMS_H_
