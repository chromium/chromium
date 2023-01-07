// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_probe_result.h"

#include "base/notreached.h"

bool PrefetchProxyProbeResultIsSuccess(PrefetchProxyProbeResult result) {
  switch (result) {
    case PrefetchProxyProbeResult::kNoProbing:
    case PrefetchProxyProbeResult::kDNSProbeSuccess:
    case PrefetchProxyProbeResult::kTLSProbeSuccess:
      return true;
    case PrefetchProxyProbeResult::kTLSProbeFailure:
    case PrefetchProxyProbeResult::kDNSProbeFailure:
      return false;
  }
  NOTREACHED();
  return false;
}
