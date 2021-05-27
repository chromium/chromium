// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_feature_usage_metrics.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "components/prefs/pref_service.h"

namespace {
const char kNearbyShareUmaFeatureName[] = "NearbyShare";
}  // namespace

NearbyShareFeatureUsageMetrics::NearbyShareFeatureUsageMetrics(
    PrefService* pref_service)
    : pref_service_(pref_service),
      feature_usage_metrics_(kNearbyShareUmaFeatureName, this) {}

NearbyShareFeatureUsageMetrics::~NearbyShareFeatureUsageMetrics() = default;

NearbyShareFeatureUsageMetrics::NearbyShareEnabledState
NearbyShareFeatureUsageMetrics::GetNearbyShareEnabledState() const {
  bool is_enabled =
      pref_service_->GetBoolean(prefs::kNearbySharingEnabledPrefName);
  bool is_managed =
      pref_service_->IsManagedPreference(prefs::kNearbySharingEnabledPrefName);
  bool is_onboarded = pref_service_->GetBoolean(
      prefs::kNearbySharingOnboardingCompletePrefName);

  if (is_enabled) {
    return is_onboarded ? NearbyShareEnabledState::kEnabledAndOnboarded
                        : NearbyShareEnabledState::kEnabledAndNotOnboarded;
  }

  if (is_managed) {
    return NearbyShareEnabledState::kDisallowedByPolicy;
  }

  return is_onboarded ? NearbyShareEnabledState::kDisabledAndOnboarded
                      : NearbyShareEnabledState::kDisabledAndNotOnboarded;
}

void NearbyShareFeatureUsageMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool NearbyShareFeatureUsageMetrics::IsEligible() const {
  // This class is only created if the Nearby Share service is started, which
  // only occurs for eligible users.
  return true;
}

bool NearbyShareFeatureUsageMetrics::IsEnabled() const {
  switch (GetNearbyShareEnabledState()) {
    case NearbyShareEnabledState::kEnabledAndOnboarded:
    case NearbyShareEnabledState::kEnabledAndNotOnboarded:
      return true;
    case NearbyShareEnabledState::kDisabledAndOnboarded:
    case NearbyShareEnabledState::kDisabledAndNotOnboarded:
    case NearbyShareEnabledState::kDisallowedByPolicy:
      return false;
  }
}
