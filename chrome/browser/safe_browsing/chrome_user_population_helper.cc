// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"

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
#include "components/sync/service/sync_service.h"
#include "components/webdata/common/web_database_service.h"

namespace safe_browsing {

namespace {

std::optional<ChromeUserPopulation>& GetCachedUserPopulation(Profile* profile) {
  static base::NoDestructor<
      std::map<Profile*, std::optional<ChromeUserPopulation>>>
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
  const std::optional<ChromeUserPopulation>& cached_population =
      GetCachedUserPopulation(profile);
  if (!cached_population) {
    return;
  }
}

}  // namespace

void ClearCachedUserPopulation(Profile* profile,
                               NoCachedPopulationReason reason) {
  GetCachedUserPopulation(profile) = std::nullopt;
  GetNoCachedPopulationReason(profile) = reason;
}

ChromeUserPopulation GetUserPopulationForProfile(Profile* profile) {
  // |profile| may be null in tests.
  if (!profile)
    return ChromeUserPopulation();

  syncer::SyncService* sync = SyncServiceFactory::GetForProfile(profile);
  bool is_history_sync_active =
      sync && !sync->IsLocalSyncEnabled() &&
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

  std::optional<size_t> num_profiles;
  std::optional<size_t> num_loaded_profiles;
  std::optional<size_t> num_open_profiles;

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
      profile->GetPrefs(), profile->IsOffTheRecord(), is_history_sync_active,
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
            &features::kLockProfileCookieDatabase,
            &features::kUseAppBoundEncryptionProviderForEncryption,
#endif
            &features::kUseNewEncryptionKeyForWebData,
        }};

    GetExperimentStatus(*kCookieTheftExperiments, &population);
  }

  return population;
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
