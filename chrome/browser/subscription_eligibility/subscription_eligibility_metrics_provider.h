// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"
#include "components/subscription_eligibility/subscription_eligibility_metrics_util.h"

namespace subscription_eligibility {

class SubscriptionEligibilityMetricsProvider : public metrics::MetricsProvider {
 public:
  SubscriptionEligibilityMetricsProvider();
  ~SubscriptionEligibilityMetricsProvider() override;

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace subscription_eligibility

#endif  // CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_
