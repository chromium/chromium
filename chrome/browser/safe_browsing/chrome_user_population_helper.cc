// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/user_population.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/sync/driver/sync_service.h"

namespace safe_browsing {

namespace {

absl::optional<ChromeUserPopulation>& GetCachedUserPopulation(
    Profile* profile) {
  static base::NoDestructor<
      std::map<Profile*, absl::optional<ChromeUserPopulation>>>
      instance;
  return (*instance)[profile];
}

NoCachedPopulationReason& GetNoCachedPopulationReason(Profile* profile) {
  static base::NoDestructor<std::map<Profile*, NoCachedPopulationReason>>
      instance;
  auto it = instance->find(profile);
  if (it == instance->end()) {
    (*instance)[profile] = NoCachedPopulationReason::kStartup;
  }

  return (*instance)[profile];
}

void ComparePopulationWithCache(Profile* profile,
                                const ChromeUserPopulation& population) {
  const absl::optional<ChromeUserPopulation>& cached_population =
      GetCachedUserPopulation(profile);
  if (!cached_population) {
    return;
  }
}

}  // namespace

void ClearCachedUserPopulation(Profile* profile,
                               NoCachedPopulationReason reason) {
  GetCachedUserPopulation(profile) = absl::nullopt;
  GetNoCachedPopulationReason(profile) = reason;
}

ChromeUserPopulation GetUserPopulationForProfile(Profile* profile) {
  // |profile| may be null in tests.
  if (!profile)
    return ChromeUserPopulation();

  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  bool is_history_sync_enabled =
      sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
      sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool is_signed_in =
      identity_manager && SyncUtils::IsPrimaryAccountSignedIn(identity_manager);

  bool is_under_advanced_protection = false;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  AdvancedProtectionStatusManager* advanced_protection_manager =
      AdvancedProtectionStatusManagerFactory::GetForProfile(profile);
  is_under_advanced_protection =
      advanced_protection_manager &&
      advanced_protection_manager->IsUnderAdvancedProtection();
#endif

  absl::optional<size_t> num_profiles;
  absl::optional<size_t> num_loaded_profiles;
  absl::optional<size_t> num_open_profiles;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // |profile_manager| may be null in tests.
  if (profile_manager) {
    num_profiles = profile_manager->GetNumberOfProfiles();
    num_loaded_profiles = profile_manager->GetLoadedProfiles().size();

    // On ChromeOS multiple profiles doesn't apply, and GetLastOpenedProfiles
    // causes crashes on ChromeOS. See https://crbug.com/1211793.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    num_open_profiles = profile_manager->GetLastOpenedProfiles().size();
#endif
  }

  ChromeUserPopulation population = GetUserPopulation(
      profile->GetPrefs(), profile->IsOffTheRecord(), is_history_sync_enabled,
      is_signed_in, is_under_advanced_protection,
      g_browser_process->browser_policy_connector(), std::move(num_profiles),
      std::move(num_loaded_profiles), std::move(num_open_profiles));

  ComparePopulationWithCache(profile, population);
  GetCachedUserPopulation(profile) = population;

  return population;
}

ChromeUserPopulation GetUserPopulationForProfileWithCookieTheftExperiments(
    Profile* profile) {
  ChromeUserPopulation population = GetUserPopulationForProfile(profile);

  if (population.user_population() ==
      ChromeUserPopulation::ENHANCED_PROTECTION) {
    static const base::NoDestructor<std::vector<const base::Feature*>>
        kCookieTheftExperiments{{
#if BUILDFLAG(IS_WIN)
            &features::kLockProfileCookieDatabase
#endif
        }};

    GetExperimentStatus(*kCookieTheftExperiments, &population);
  }

  return population;
}

void GetExperimentStatus(const std::vector<const base::Feature*>& experiments,
                         ChromeUserPopulation* population) {
  for (const base::Feature* feature : experiments) {
    base::FieldTrial* field_trial = base::FeatureList::GetFieldTrial(*feature);
    if (!field_trial) {
      continue;
    }
    const std::string& trial = field_trial->trial_name();
    const std::string& group = field_trial->GetGroupNameWithoutActivation();
    bool is_experimental = group.find("Enabled") != std::string::npos ||
                           group.find("Control") != std::string::npos;
    bool is_preperiod = group.find("Preperiod") != std::string::npos;
    if (is_experimental && !is_preperiod) {
      population->add_finch_active_groups(trial + "." + group);
    }
  }
}

ChromeUserPopulation::PageLoadToken GetPageLoadTokenForURL(Profile* profile,
                                                           GURL url) {
  if (!profile) {
    return ChromeUserPopulation::PageLoadToken();
  }
  VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(profile);
  if (!cache_manager) {
    return ChromeUserPopulation::PageLoadToken();
  }

  ChromeUserPopulation::PageLoadToken token =
      cache_manager->GetPageLoadToken(url);
  if (token.has_token_value()) {
    return token;
  }
  return cache_manager->CreatePageLoadToken(url);
}

}  // namespace safe_browsing
