// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subscription_eligibility/subscription_eligibility_metrics_provider.h"

#include <set>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/variations/synthetic_trials.h"
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

  AiSubscriptionTierStatus status =
      ComputeAiSubscriptionTierStatus(subscription_tiers);

  CHECK_NE(status, AiSubscriptionTierStatus::kValueNotSet);
  base::UmaHistogramEnumeration(kAiSubscriptionTierStatusHistogramName, status);

  std::string group_name = GetSyntheticTrialGroupName(status);
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      kAiSubscriptionTierSyntheticTrialName, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

}  // namespace subscription_eligibility
