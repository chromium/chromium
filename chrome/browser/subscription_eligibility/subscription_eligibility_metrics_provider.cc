// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_metrics_provider.h"

#include <set>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace subscription_eligibility {

SubscriptionEligibilityMetricsProvider::
    SubscriptionEligibilityMetricsProvider() = default;
SubscriptionEligibilityMetricsProvider::
    ~SubscriptionEligibilityMetricsProvider() = default;

void SubscriptionEligibilityMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return;
  }

  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
  if (profile_list.empty()) {
    return;
  }

  std::set<int32_t> subscription_tiers;
  for (auto* profile : profile_list) {
    auto* subscription_eligibility_service =
        SubscriptionEligibilityServiceFactory::GetForProfile(profile);
    int32_t profile_subscription_tier =
        subscription_eligibility_service
            ? subscription_eligibility_service->GetAiSubscriptionTier()
            : 0;
    subscription_tiers.insert(
        profile_subscription_tier >= 0 ? profile_subscription_tier : 0);
  }

  AiSubscriptionTierStatus status = AiSubscriptionTierStatus::kValueNotSet;
  bool nonzero_no_subscription =
      subscription_tiers.find(0) != subscription_tiers.end();
  if (subscription_tiers.size() == 1 && nonzero_no_subscription) {
    // All profiles not enabled.
    status = AiSubscriptionTierStatus::kNoProfilesSubscribed;
  } else if (subscription_tiers.size() > 1 && nonzero_no_subscription) {
    // Some profiles enabled and some not enabled.
    status = AiSubscriptionTierStatus::kSomeProfilesSubscribed;
  } else if (subscription_tiers.size() > 1 && !nonzero_no_subscription) {
    // All profiles enabled but at different tiers.
    status = AiSubscriptionTierStatus::kAllProfilesSubscribedButDifferentTiers;
  } else {
    CHECK(subscription_tiers.size() == 1);
    CHECK(!nonzero_no_subscription);

    if (subscription_tiers.contains(1)) {
      // All profiles enabled but at tier = 1.
      status = AiSubscriptionTierStatus::kAllProfilesAtTierEquals1;
    } else if (subscription_tiers.contains(2)) {
      // All profiles enabled but at tier = 2.
      status = AiSubscriptionTierStatus::kAllProfilesAtTierEquals2;
    } else if (subscription_tiers.contains(3)) {
      status = AiSubscriptionTierStatus::kAllProfilesAtTierEquals3;
    } else {
      // All profiles enabled but at unknown tier.
      status = AiSubscriptionTierStatus::kAllProfilesSubscribedForUnknownTier;
    }
  }

  CHECK_NE(status, AiSubscriptionTierStatus::kValueNotSet);
  base::UmaHistogramEnumeration(
      "SubscriptionEligibility.AiSubscriptionTierStatus", status);
}

}  // namespace subscription_eligibility
