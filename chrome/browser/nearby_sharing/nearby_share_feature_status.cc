// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "components/prefs/pref_service.h"

NearbyShareEnabledState GetNearbyShareEnabledState(PrefService* pref_service) {
  bool is_enabled =
      pref_service->GetBoolean(prefs::kNearbySharingEnabledPrefName);
  bool is_managed =
      pref_service->IsManagedPreference(prefs::kNearbySharingEnabledPrefName);
  bool is_onboarded =
      pref_service->GetBoolean(prefs::kNearbySharingOnboardingCompletePrefName);

  if (is_enabled) {
    return is_onboarded ? NearbyShareEnabledState::kEnabledAndOnboarded
                        : NearbyShareEnabledState::kEnabledAndNotOnboarded;
  }

  // A managed pref is set by an admin policy, and because managed prefs
  // have the highest priority, this also indicates whether the pref is
  // actually being controlled by the policy setting. We only care when
  // Nearby Share is disallowed by policy because the feature needs to be
  // off and unchangeable by the user. If the Nearby Share is allowed by
  // policy, the user can choose whether to enable or disable.
  if (is_managed) {
    return NearbyShareEnabledState::kDisallowedByPolicy;
  }

  return is_onboarded ? NearbyShareEnabledState::kDisabledAndOnboarded
                      : NearbyShareEnabledState::kDisabledAndNotOnboarded;
}
