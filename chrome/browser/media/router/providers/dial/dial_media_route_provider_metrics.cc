// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace media_router {

// static
void DialMediaRouteProviderMetrics::RecordCreateRouteResult(
    DialCreateRouteResult result) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramDialCreateRouteResult, result,
                            DialCreateRouteResult::kCount);
}

}  // namespace media_router
