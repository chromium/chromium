// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace {
// Stores whether this is a Google Chrome-branded build.
bool g_is_chrome_build =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    true;
#else
    false;
#endif

search_engines::SearchEngineChoiceScreenConditions ComputeProfileEligibility(
    Profile& profile) {
  if (!search_engines::IsChoiceScreenFlagEnabled(
          search_engines::ChoicePromo::kAny)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kFeatureSuppressed;
  }

  if (!SearchEngineChoiceServiceFactory::IsSelectedChoiceProfile(
          profile, /*try_claim=*/false)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kProfileOutOfScope;
  }

  bool is_regular_profile = profile.IsRegularProfile();
#if BUILDFLAG(IS_CHROMEOS)
  is_regular_profile &= !chromeos::IsManagedGuestSession() &&
                        !chromeos::IsKioskSession() &&
                        !profiles::IsChromeAppKioskSession();
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  is_regular_profile &= !profile.IsGuestSession();
#endif

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);

  return search_engines::GetStaticChoiceScreenConditions(
      CHECK_DEREF(g_browser_process->policy_service()),
      /*profile_properties=*/
      {.is_regular_profile = is_regular_profile,
       .pref_service = profile.GetPrefs()},
      CHECK_DEREF(template_url_service));
}

bool IsProfileEligibleForChoiceScreen(Profile& profile) {
  auto eligibility_conditions = ComputeProfileEligibility(profile);
  RecordChoiceScreenProfileInitCondition(eligibility_conditions);
  return eligibility_conditions ==
         search_engines::SearchEngineChoiceScreenConditions::kEligible;
}

}  // namespace

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .WithGuest(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

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

// static
base::AutoReset<bool>
SearchEngineChoiceServiceFactory::ScopedChromeBuildOverrideForTesting(
    bool force_chrome_build) {
  CHECK_IS_TEST();
  return base::AutoReset<bool>(&g_is_chrome_build, force_chrome_build);
}

// static
bool SearchEngineChoiceServiceFactory::IsSelectedChoiceProfile(Profile& profile,
                                                               bool try_claim) {
  // TODO(b/309936758): Remove this method and deprecate
  // prefs::kSearchEnginesChoiceProfile
  return true;
}

// static
bool SearchEngineChoiceServiceFactory::
    IsProfileEligibleForChoiceScreenForTesting(Profile& profile) {
  CHECK_IS_TEST();
  return IsProfileEligibleForChoiceScreen(profile);
}

std::unique_ptr<KeyedService>
SearchEngineChoiceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!g_is_chrome_build) {
    return nullptr;
  }

  auto& profile = CHECK_DEREF(Profile::FromBrowserContext(context));
  if (!IsProfileEligibleForChoiceScreen(profile)) {
    return nullptr;
  }

  TemplateURLService& template_url_service =
      CHECK_DEREF(TemplateURLServiceFactory::GetForProfile(&profile));
  return std::make_unique<SearchEngineChoiceService>(profile,
                                                     template_url_service);
}
