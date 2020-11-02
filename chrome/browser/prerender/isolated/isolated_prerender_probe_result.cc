// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_probe_result.h"

#include "base/notreached.h"

bool IsolatedPrerenderProbeResultIsSuccess(
    IsolatedPrerenderProbeResult result) {
  switch (result) {
    case IsolatedPrerenderProbeResult::kNoProbing:
    case IsolatedPrerenderProbeResult::kDNSProbeSuccess:
    case IsolatedPrerenderProbeResult::kTLSProbeSuccess:
      return true;
    case IsolatedPrerenderProbeResult::kTLSProbeFailure:
    case IsolatedPrerenderProbeResult::kDNSProbeFailure:
      return false;
  }
  NOTREACHED();
  return false;
}
