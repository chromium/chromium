// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_METRICS_H_

namespace media_router {

static constexpr char kHistogramDialCreateRouteResult[] =
    "MediaRouter.Dial.CreateRoute";

// Note on enums defined in this file:
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. They must also be kept in sync with
// tools/metrics/histograms/enums.xml.

enum class DialCreateRouteResult {
  kSuccess = 0,
  kSinkNotFound = 1,
  kAppInfoNotFound = 2,
  kAppLaunchFailed = 3,
  kUnsupportedSource = 4,
  kRouteAlreadyExists = 5,
  kCount
};

class DialMediaRouteProviderMetrics {
 public:
  static void RecordCreateRouteResult(DialCreateRouteResult result);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_METRICS_H_
