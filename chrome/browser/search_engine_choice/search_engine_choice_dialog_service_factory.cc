// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
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
  bool is_regular_or_guest_profile =
      profile.IsRegularProfile() || profile.IsGuestSession();
#if BUILDFLAG(IS_CHROMEOS)
  is_regular_or_guest_profile &=
      !chromeos::IsKioskSession() && !profiles::IsChromeAppKioskSession();
#endif

  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(&profile);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  if (!template_url_service) {
    // Some unit tests using `BrowserWithTestWindowTest` create browser windows
    // without fully instantiating profiles.
    CHECK_IS_TEST();
    return search_engines::SearchEngineChoiceScreenConditions::
        kUnsupportedBrowserType;
  }

  return search_engine_choice_service->GetStaticChoiceScreenConditions(
      CHECK_DEREF(g_browser_process->policy_service()),
      is_regular_or_guest_profile, *template_url_service);
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

SearchEngineChoiceDialogServiceFactory::SearchEngineChoiceDialogServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchEngineChoiceDialogServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(search_engines::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchEngineChoiceDialogServiceFactory::
    ~SearchEngineChoiceDialogServiceFactory() = default;

// static
SearchEngineChoiceDialogServiceFactory*
SearchEngineChoiceDialogServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceDialogServiceFactory> factory;
  return factory.get();
}

// static
SearchEngineChoiceDialogService*
SearchEngineChoiceDialogServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchEngineChoiceDialogService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
base::AutoReset<bool>
SearchEngineChoiceDialogServiceFactory::ScopedChromeBuildOverrideForTesting(
    bool force_chrome_build) {
  CHECK_IS_TEST();
  return base::AutoReset<bool>(&g_is_chrome_build, force_chrome_build);
}

// static
bool SearchEngineChoiceDialogServiceFactory::
    IsProfileEligibleForChoiceScreenForTesting(Profile& profile) {
  CHECK_IS_TEST();
  return IsProfileEligibleForChoiceScreen(profile);
}

std::unique_ptr<KeyedService>
SearchEngineChoiceDialogServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(CHROME_FOR_TESTING)
  return nullptr;
#else

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  if (!g_is_chrome_build &&
      !command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    return nullptr;
  }

  if (command_line->HasSwitch(switches::kNoFirstRun) &&
      !command_line->HasSwitch(
          switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen)) {
    return nullptr;
  }

  Profile& profile = CHECK_DEREF(Profile::FromBrowserContext(context));
  search_engines::SearchEngineChoiceService& search_engine_choice_service =
      CHECK_DEREF(
          search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
              &profile));

  if (!IsProfileEligibleForChoiceScreen(profile)) {
    return nullptr;
  }

  TemplateURLService& template_url_service =
      CHECK_DEREF(TemplateURLServiceFactory::GetForProfile(&profile));
  return std::make_unique<SearchEngineChoiceDialogService>(
      profile, search_engine_choice_service, template_url_service);
#endif
}
