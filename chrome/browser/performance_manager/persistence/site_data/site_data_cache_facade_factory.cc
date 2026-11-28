// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

SiteDataCacheFacadeFactory* SiteDataCacheFacadeFactory::GetInstance() {
  static base::NoDestructor<SiteDataCacheFacadeFactory> instance;
  return instance.get();
}

SiteDataCacheFacade* SiteDataCacheFacadeFactory::GetProfileFacadeForTesting(
    Profile* profile) {
  return static_cast<SiteDataCacheFacade*>(
      GetServiceForBrowserContext(profile, /*create=*/true));
}

SiteDataCacheFacadeFactory::SiteDataCacheFacadeFactory()
    : ProfileKeyedServiceFactory(
          "SiteDataCacheFacadeFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

SiteDataCacheFacadeFactory::~SiteDataCacheFacadeFactory() = default;

std::unique_ptr<KeyedService>
SiteDataCacheFacadeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M163);
  return std::make_unique<SiteDataCacheFacade>(context);
}

bool SiteDataCacheFacadeFactory::ServiceIsCreatedWithBrowserContext() const {
  // It's fine to initialize this service when the browser context
  // gets created so the database will be ready when we need it.
  return true;
}

bool SiteDataCacheFacadeFactory::ServiceIsNULLWhileTesting() const {
  // Unit tests that need a SiteDataCacheFacade create it directly.
  return true;
}

void SiteDataCacheFacadeFactory::OnBeforeFacadeCreated(
    base::PassKey<SiteDataCacheFacade>) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M163);
  if (service_instance_count_ == 0U) {
    CHECK(!cache_factory_, base::NotFatalUntil::M163);
    cache_factory_ = std::make_unique<SiteDataCacheFactory>();
  }
  ++service_instance_count_;
}

void SiteDataCacheFacadeFactory::OnFacadeDestroyed(
    base::PassKey<SiteDataCacheFacade>) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M163);
  CHECK_GT(service_instance_count_, 0U, base::NotFatalUntil::M163);
  // Destroy the cache factory if there's no more SiteDataCacheFacade needing
  // it.
  if (--service_instance_count_ == 0) {
    cache_factory_ = nullptr;
  }
}

}  // namespace performance_manager
