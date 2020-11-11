// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. If entries are added, kMaxValue should
// be updated.
enum class NearbyShareEnabledState {
  kEnabledAndOnboarded = 0,
  kEnabledAndNotOnboarded = 1,
  kDisabledAndOnboarded = 2,
  kDisabledAndNotOnboarded = 3,
  kDisallowedByPolicy = 4,
  kMaxValue = kDisallowedByPolicy
};

}  // namespace

void RecordNearbyShareEnabledMetric(const PrefService* pref_service) {
  NearbyShareEnabledState state;

  bool is_managed =
      pref_service->IsManagedPreference(prefs::kNearbySharingEnabledPrefName);
  bool is_enabled =
      pref_service->GetBoolean(prefs::kNearbySharingEnabledPrefName);
  bool is_onboarded =
      pref_service->GetBoolean(prefs::kNearbySharingOnboardingCompletePrefName);

  if (is_enabled) {
    state = is_onboarded ? NearbyShareEnabledState::kEnabledAndOnboarded
                         : NearbyShareEnabledState::kEnabledAndNotOnboarded;
  } else if (is_managed) {
    state = NearbyShareEnabledState::kDisallowedByPolicy;
  } else {  // !is_enabled && !is_managed
    state = is_onboarded ? NearbyShareEnabledState::kDisabledAndOnboarded
                         : NearbyShareEnabledState::kDisabledAndNotOnboarded;
  }

  base::UmaHistogramEnumeration("Nearby.Share.Enabled", state);
}
