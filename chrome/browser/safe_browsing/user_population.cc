// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/user_population.h"

#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

ChromeUserPopulation GetUserPopulation(Profile* profile) {
  ChromeUserPopulation population;

  // |profile| may be null in tests.
  if (!profile)
    return population;

  if (profile->GetPrefs()) {
    const PrefService& prefs = *profile->GetPrefs();
    if (IsEnhancedProtectionEnabled(prefs)) {
      population.set_user_population(ChromeUserPopulation::ENHANCED_PROTECTION);
    } else if (IsExtendedReportingEnabled(prefs)) {
      population.set_user_population(ChromeUserPopulation::EXTENDED_REPORTING);
    } else if (IsSafeBrowsingEnabled(prefs)) {
      population.set_user_population(ChromeUserPopulation::SAFE_BROWSING);
    }

    population.set_is_mbb_enabled(prefs.GetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled));
  }

  population.set_is_incognito(profile->IsOffTheRecord());

  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  bool is_history_sync_enabled =
      sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
  population.set_is_history_sync_enabled(is_history_sync_enabled);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  AdvancedProtectionStatusManager* advanced_protection_manager =
      AdvancedProtectionStatusManagerFactory::GetForProfile(profile);
  population.set_is_under_advanced_protection(
      advanced_protection_manager &&
      advanced_protection_manager->IsUnderAdvancedProtection());
#endif

  population.set_profile_management_status(GetProfileManagementStatus(
      g_browser_process->browser_policy_connector()));

  if (base::FeatureList::IsEnabled(kBetterTelemetryAcrossReports)) {
    std::string user_agent =
        version_info::GetProductNameAndVersionForUserAgent() + "/" +
        version_info::GetOSType();
    population.set_user_agent(user_agent);

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // |profile_manager| may be null in tests.
    if (profile_manager) {
      population.set_number_of_profiles(profile_manager->GetNumberOfProfiles());
      population.set_number_of_loaded_profiles(
          profile_manager->GetLoadedProfiles().size());
// On ChromeOS multiple profiles doesn't apply, and GetLastOpenedProfiles causes
// crashes on ChromeOS. See https://crbug.com/1211793.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
      population.set_number_of_open_profiles(
          profile_manager->GetLastOpenedProfiles().size());
#endif
    }
  }

  return population;
}

}  // namespace safe_browsing
