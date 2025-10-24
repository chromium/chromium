// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

namespace subscription_eligibility {

enum class AiSubscriptionTierStatus {
  kValueNotSet = 0,
  // No profiles have an AI subscription tier.
  kNoProfilesSubscribed = 1,
  // Some profiles have an AI subscription tier, some do not.
  kSomeProfilesSubscribed = 2,
  // All profiles have an AI subscription tier but are different.
  kAllProfilesSubscribedButDifferentTiers = 3,
  // All profiles have the same AI subscription tier but for a tier not known to
  // this browser.
  kAllProfilesSubscribedForUnknownTier = 4,
  // All profiles subscribed for tier=1 AI subscription tier.
  kAllProfilesAtTierEquals1 = 5,
  // All profiles subscribed for tier=2 AI subscription tier.
  kAllProfilesAtTierEquals2 = 6,
  // All profiles subscribed for tier=3 AI subscription tier.
  kAllProfilesAtTierEquals3 = 7,

  // Values must not be deleted or repurposed. Must be kept in sync with
  // SubscriptionEligibilityAiSubscriptionTierStatus in others.enums.xml.

  // New values above this line.
  kMaxValue = kAllProfilesAtTierEquals3,
};

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
