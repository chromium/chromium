// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROBE_RESULT_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROBE_RESULT_H_

// The result of an origin probe. See PrefetchProxyOriginProber.
enum class PrefetchProxyProbeResult {
  kNoProbing = 0,
  kDNSProbeSuccess = 1,
  kDNSProbeFailure = 2,
  kTLSProbeSuccess = 3,
  kTLSProbeFailure = 4,
};

// Returns true if the probe result is not a failure.
bool PrefetchProxyProbeResultIsSuccess(PrefetchProxyProbeResult result);

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROBE_RESULT_H_
