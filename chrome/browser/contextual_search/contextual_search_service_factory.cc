// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/version_info/version_info.h"

// static
contextual_search::ContextualSearchService*
ContextualSearchServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<contextual_search::ContextualSearchService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContextualSearchServiceFactory* ContextualSearchServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualSearchServiceFactory> instance;
  return instance.get();
}

ContextualSearchServiceFactory::ContextualSearchServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContextualSearchService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

ContextualSearchServiceFactory::~ContextualSearchServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextualSearchServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<contextual_search::ContextualSearchService>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(),
      TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetVariationsClient(), chrome::GetChannel(),
      g_browser_process->GetApplicationLocale());
}
