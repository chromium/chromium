// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_feature_usage_metrics.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "components/prefs/pref_service.h"

namespace {
const char kNearbyShareUmaFeatureName[] = "NearbyShare";
}  // namespace

NearbyShareFeatureUsageMetrics::NearbyShareFeatureUsageMetrics(
    PrefService* pref_service)
    : pref_service_(pref_service),
      feature_usage_metrics_(kNearbyShareUmaFeatureName, this) {}

NearbyShareFeatureUsageMetrics::~NearbyShareFeatureUsageMetrics() = default;

void NearbyShareFeatureUsageMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool NearbyShareFeatureUsageMetrics::IsEligible() const {
  // This class is only created if the Nearby Share service is started, which
  // only occurs for eligible users.
  return true;
}

bool NearbyShareFeatureUsageMetrics::IsEnabled() const {
  switch (GetNearbyShareEnabledState(pref_service_)) {
    case NearbyShareEnabledState::kEnabledAndOnboarded:
    case NearbyShareEnabledState::kEnabledAndNotOnboarded:
      return true;
    case NearbyShareEnabledState::kDisabledAndOnboarded:
    case NearbyShareEnabledState::kDisabledAndNotOnboarded:
    case NearbyShareEnabledState::kDisallowedByPolicy:
      return false;
  }
}
