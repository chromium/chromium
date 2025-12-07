// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/regional_capabilities/regional_capabilities_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_client.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data_resolver_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/variations/service/variations_service.h"

namespace search_engines {
namespace {
std::unique_ptr<KeyedService> BuildSearchEngineChoiceService(
    content::BrowserContext* context) {
  Profile& profile = CHECK_DEREF(Profile::FromBrowserContext(context));

  auto service = std::make_unique<SearchEngineChoiceService>(
      std::make_unique<SearchEngineChoiceServiceClient>(profile),
      CHECK_DEREF(profile.GetPrefs()), g_browser_process->local_state(),
      CHECK_DEREF(regional_capabilities::RegionalCapabilitiesServiceFactory::
                      GetForProfile(&profile)),
      CHECK_DEREF(
          TemplateURLPrepopulateData::ResolverFactory::GetForProfile(&profile)),
      CHECK_DEREF(IdentityManagerFactory::GetForProfile(&profile)),
      CHECK_DEREF(policy::ManagementServiceFactory::GetForPlatform()));
  service->Init();
  return service;
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
              .Build()) {
  DependsOn(
      regional_capabilities::RegionalCapabilitiesServiceFactory::GetInstance());
  DependsOn(TemplateURLPrepopulateData::ResolverFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());
}

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

void SearchEngineChoiceServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SearchEngineChoiceService::RegisterProfilePrefs(registry);
}

}  // namespace search_engines
