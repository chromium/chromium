// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace performance_manager {

// static
SiteDataCacheFacade* SiteDataCacheFacadeFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SiteDataCacheFacade*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SiteDataCacheFacadeFactory* SiteDataCacheFacadeFactory::GetInstance() {
  static base::NoDestructor<SiteDataCacheFacadeFactory> instance;
  return instance.get();
}

SiteDataCacheFacadeFactory::SiteDataCacheFacadeFactory()
    : BrowserContextKeyedServiceFactory(
          "SiteDataCacheFacadeFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

SiteDataCacheFacadeFactory::~SiteDataCacheFacadeFactory() = default;

KeyedService* SiteDataCacheFacadeFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SiteDataCacheFacade(context);
}

content::BrowserContext* SiteDataCacheFacadeFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool SiteDataCacheFacadeFactory::ServiceIsCreatedWithBrowserContext() const {
  // It's fine to initialize this service when the browser context
  // gets created so the database will be ready when we need it.
  return true;
}

bool SiteDataCacheFacadeFactory::ServiceIsNULLWhileTesting() const {
  // Tests that want to use this factory will have to explicitly enable it.
  return true;
}

}  // namespace performance_manager
