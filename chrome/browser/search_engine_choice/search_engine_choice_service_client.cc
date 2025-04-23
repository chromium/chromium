// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_client.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
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

}  // namespace search_engines
