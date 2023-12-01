// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
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

  bool is_regular_or_guest_profile =
      profile.IsRegularProfile() || profile.IsGuestSession();
#if BUILDFLAG(IS_CHROMEOS)
  is_regular_or_guest_profile &=
      !chromeos::IsKioskSession() && !profiles::IsChromeAppKioskSession();
#endif

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);

  return search_engines::GetStaticChoiceScreenConditions(
      CHECK_DEREF(g_browser_process->policy_service()),
      /*profile_properties=*/
      {.is_regular_profile = is_regular_or_guest_profile,
       .pref_service = profile.GetPrefs()},
      CHECK_DEREF(template_url_service));
}

bool IsProfileEligibleForChoiceScreen(Profile& profile) {
  auto eligibility_conditions = ComputeProfileEligibility(profile);
  // TODO(b/312755450): Move metrics recording outside of this function or
  // rename it to not appear like a simple getter.
  RecordChoiceScreenProfileInitCondition(eligibility_conditions);
  DVLOG(1) << "Choice screen eligibility condition for profile "
           << profile.GetBaseName() << ": "
           << static_cast<int>(eligibility_conditions);
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
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
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
  search_engines::PreprocessPrefsForReprompt(CHECK_DEREF(profile.GetPrefs()));

  if (!IsProfileEligibleForChoiceScreen(profile)) {
    DVLOG(1) << "Profile not eligible, removing tag for profile "
             << profile.GetBaseName();
    profile.GetPrefs()->ClearPref(prefs::kDefaultSearchProviderChoicePending);
    return nullptr;
  }

  TemplateURLService& template_url_service =
      CHECK_DEREF(TemplateURLServiceFactory::GetForProfile(&profile));
  return std::make_unique<SearchEngineChoiceService>(profile,
                                                     template_url_service);
}
