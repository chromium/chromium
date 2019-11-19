// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader_impl.h"
#include "chrome/browser/search/search_suggest/search_suggest_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
SearchSuggestService* SearchSuggestServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SearchSuggestService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SearchSuggestServiceFactory* SearchSuggestServiceFactory::GetInstance() {
  return base::Singleton<SearchSuggestServiceFactory>::get();
}

SearchSuggestServiceFactory::SearchSuggestServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SearchSuggestService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

SearchSuggestServiceFactory::~SearchSuggestServiceFactory() = default;

KeyedService* SearchSuggestServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!SearchSuggestService::IsEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new SearchSuggestService(
      profile, identity_manager,
      std::make_unique<SearchSuggestLoaderImpl>(
          url_loader_factory, g_browser_process->GetApplicationLocale()));
}
