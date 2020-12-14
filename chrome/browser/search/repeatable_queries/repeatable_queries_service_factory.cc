// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/repeatable_queries/repeatable_queries_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/search/repeatable_queries/repeatable_queries_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
RepeatableQueriesService* RepeatableQueriesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<RepeatableQueriesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RepeatableQueriesServiceFactory*
RepeatableQueriesServiceFactory::GetInstance() {
  return base::Singleton<RepeatableQueriesServiceFactory>::get();
}

RepeatableQueriesServiceFactory::RepeatableQueriesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RepeatableQueriesService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

RepeatableQueriesServiceFactory::~RepeatableQueriesServiceFactory() = default;

KeyedService* RepeatableQueriesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpRepeatableQueries))
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new RepeatableQueriesService(identity_manager, history_service,
                                      template_url_service, url_loader_factory,
                                      GURL(chrome::kChromeUINewTabURL));
}
