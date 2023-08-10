// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"

#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/signin/public/base/signin_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

SearchEngineChoiceServiceFactory::~SearchEngineChoiceServiceFactory() = default;

// static
SearchEngineChoiceServiceFactory*
SearchEngineChoiceServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceServiceFactory> factory;
  return factory.get();
}

// static
SearchEngineChoiceService* SearchEngineChoiceServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SearchEngineChoiceService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

bool SearchEngineChoiceServiceFactory::IsProfileEligibleForChoiceScreen(
    const policy::PolicyService& policy_service,
    Profile& profile) const {
  if (!base::FeatureList::IsEnabled(switches::kSearchEngineChoice)) {
    return false;
  }

  bool is_regular_profile = profile.IsRegularProfile();
#if BUILDFLAG(IS_CHROMEOS)
  is_regular_profile &= !profiles::IsPublicSession() &&
                        !chromeos::IsKioskSession() &&
                        !profiles::IsChromeAppKioskSession();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  is_regular_profile &= !profile.IsGuestSession();
#endif

  return search_engines::ShouldShowChoiceScreen(
      policy_service,
      /*profile_properties=*/{.is_regular_profile = is_regular_profile,
                              .pref_service = profile.GetPrefs()});
}

KeyedService* SearchEngineChoiceServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto& profile = CHECK_DEREF(Profile::FromBrowserContext(context));

  if (!IsProfileEligibleForChoiceScreen(
          CHECK_DEREF(g_browser_process->policy_service()), profile)) {
    return nullptr;
  }
  return new SearchEngineChoiceService();
}
