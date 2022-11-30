// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_HTTPS_ENGAGEMENT_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_HTTPS_ENGAGEMENT_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// When user metrics are about to be uploaded, HttpsEngagementMetricsProvider
// looks up the HttpsEngagementService for the current profile and tells it to
// record its metrics before the upload occurs.
class HttpsEngagementMetricsProvider : public metrics::MetricsProvider {
 public:
  HttpsEngagementMetricsProvider();
  ~HttpsEngagementMetricsProvider() override;

  // metrics:MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  // CHROME_BROWSER_METRICS_HTTPS_ENGAGEMENT_METRICS_PROVIDER_H_
