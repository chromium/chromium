// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROBE_RESULT_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROBE_RESULT_H_

// The result of an origin probe. See IsolatedPrerenderOriginProber.
enum class IsolatedPrerenderProbeResult {
  kNoProbing = 0,
  kDNSProbeSuccess = 1,
  kDNSProbeFailure = 2,
  kTLSProbeSuccess = 3,
  kTLSProbeFailure = 4,
};

// Returns true if the probe result is not a failure.
bool IsolatedPrerenderProbeResultIsSuccess(IsolatedPrerenderProbeResult result);

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PROBE_RESULT_H_
