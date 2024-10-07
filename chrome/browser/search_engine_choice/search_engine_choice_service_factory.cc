// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace search_engines {
namespace {
std::unique_ptr<KeyedService> BuildSearchEngineChoiceService(
    content::BrowserContext* context) {
  Profile& profile = CHECK_DEREF(Profile::FromBrowserContext(context));

  bool is_profile_elibile_for_dse_guest_propagation = false;
#if !BUILDFLAG(IS_ANDROID)
  is_profile_elibile_for_dse_guest_propagation =
      base::FeatureList::IsEnabled(
          switches::kSearchEngineChoiceGuestExperience) &&
#if BUILDFLAG(IS_CHROMEOS)
      !chromeos::IsManagedGuestSession() &&
#endif
      profile.IsGuestSession();
#endif

  return std::make_unique<SearchEngineChoiceService>(
      CHECK_DEREF(profile.GetPrefs()), g_browser_process->local_state(),
      is_profile_elibile_for_dse_guest_propagation,
      g_browser_process->variations_service());
}
}  // namespace

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

SearchEngineChoiceServiceFactory::~SearchEngineChoiceServiceFactory() = default;

// static
SearchEngineChoiceService* SearchEngineChoiceServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SearchEngineChoiceService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SearchEngineChoiceServiceFactory*
SearchEngineChoiceServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceServiceFactory> factory;
  return factory.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
SearchEngineChoiceServiceFactory::GetDefaultFactory() {
  CHECK_IS_TEST();
  return base::BindRepeating(&BuildSearchEngineChoiceService);
}

std::unique_ptr<KeyedService>
SearchEngineChoiceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildSearchEngineChoiceService(context);
}
}  // namespace search_engines
