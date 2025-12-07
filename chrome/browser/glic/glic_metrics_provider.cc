// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics_provider.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace glic {

GlicMetricsProvider::GlicMetricsProvider() = default;
GlicMetricsProvider::~GlicMetricsProvider() = default;

void GlicMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    return;
  }

  std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
  int num_enabled_profiles_enabled_for_tiered_rollout = 0;
  int num_enabled_profiles = 0;
  for (auto* profile : profile_list) {
    if (GlicEnabling::IsEnabledForProfile(profile)) {
      num_enabled_profiles++;
      if (GlicEnabling::IsEligibleForGlicTieredRollout(profile)) {
        num_enabled_profiles_enabled_for_tiered_rollout++;
      }
    }
  }

  // No profiles enabled.
  if (num_enabled_profiles == 0) {
    return;
  }

  GlicTieredRolloutEnablementStatus enablement_status;
  if (num_enabled_profiles == num_enabled_profiles_enabled_for_tiered_rollout) {
    enablement_status = GlicTieredRolloutEnablementStatus::kAllProfilesEnabled;
  } else if (num_enabled_profiles_enabled_for_tiered_rollout == 0) {
    enablement_status = GlicTieredRolloutEnablementStatus::kNoProfilesEnabled;
  } else {
    enablement_status = GlicTieredRolloutEnablementStatus::kSomeProfilesEnabled;
  }

  base::UmaHistogramEnumeration("Glic.TieredRolloutEnablementStatus",
                                enablement_status);
}

}  // namespace glic
