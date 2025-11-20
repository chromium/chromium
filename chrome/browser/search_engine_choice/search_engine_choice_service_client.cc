// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_client.h"

#include "base/check_is_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace search_engines {

SearchEngineChoiceServiceClient::SearchEngineChoiceServiceClient(
    const Profile& profile)
    : is_profile_eligible_for_dse_guest_propagation_(
#if BUILDFLAG(IS_CHROMEOS)
          !chromeos::IsManagedGuestSession() &&
#endif
          profile.IsGuestSession()) {
#if BUILDFLAG(IS_ANDROID)
  // We don't expect Guest profiles to be supported on Android.
  CHECK(!is_profile_eligible_for_dse_guest_propagation_);
#endif
}

country_codes::CountryId
SearchEngineChoiceServiceClient::GetVariationsCountry() {
  return GetVariationsLatestCountry(g_browser_process->variations_service());
}

bool SearchEngineChoiceServiceClient::
    IsProfileEligibleForDseGuestPropagation() {
  return is_profile_eligible_for_dse_guest_propagation_;
}

bool SearchEngineChoiceServiceClient::
    IsDeviceRestoreDetectedInCurrentSession() {
  const metrics::ClonedInstallDetector* cloned_install_detector =
      g_browser_process->GetMetricsServicesManager()
          ? &g_browser_process->GetMetricsServicesManager()
                 ->GetClonedInstallDetector()
          : nullptr;
  if (!cloned_install_detector) {
    CHECK_IS_TEST();
    return false;
  }

  return cloned_install_detector->ClonedInstallDetectedInCurrentSession();
}

bool SearchEngineChoiceServiceClient::DoesChoicePredateDeviceRestore(
    const ChoiceCompletionMetadata& choice_metadata) {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    CHECK_IS_TEST();
    return false;
  }

  metrics::ClonedInstallInfo cloned =
      metrics::ClonedInstallDetector::ReadClonedInstallInfo(local_state);
  if (!cloned.last_detection_timestamp) {
    // When a user opts out of UMA, the metrics system clears the pref tracking
    // when the clone detection happened. In this case we can't track the clone
    // status and we then can't use it to invalidate DSE choices.
    // If that has been used in the first session after the one that detected
    // the clone, the search engine choice invalidation would have been done
    // already, and the absence of this timestamp is not an issue.
    return false;
  }

  return choice_metadata.timestamp.ToTimeT() < cloned.last_detection_timestamp;
}

}  // namespace search_engines
