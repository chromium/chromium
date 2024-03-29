// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/browser_process.h"
#include "components/variations/service/variations_service.h"

namespace search_engines {
namespace {
std::unique_ptr<KeyedService> BuildSearchEngineChoiceService(
    content::BrowserContext* context) {
  int variations_country_id = country_codes::kCountryIDUnknown;
  if (g_browser_process->variations_service()) {
    variations_country_id =
        country_codes::CountryStringToCountryID(base::ToUpperASCII(
            g_browser_process->variations_service()->GetLatestCountry()));
  }

  auto& profile = CHECK_DEREF(Profile::FromBrowserContext(context));
  return std::make_unique<SearchEngineChoiceService>(*profile.GetPrefs(),
                                                     variations_country_id);
}
}  // namespace

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithSystem(ProfileSelection::kNone)
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
