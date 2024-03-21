// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade_factory.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

namespace {
// Tests that want to use this factory will have to explicitly enable it.
bool g_enable_for_testing = false;
}

SiteDataCacheFacadeFactory* SiteDataCacheFacadeFactory::GetInstance() {
  static base::NoDestructor<SiteDataCacheFacadeFactory> instance;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return instance.get();
}

// static
std::unique_ptr<base::AutoReset<bool>>
SiteDataCacheFacadeFactory::EnableForTesting() {
  // Only one AutoReset served by this function can exists, otherwise the first
  // one being released would set g_enable_for_testing to false while there's
  // other AutoReset still existing.
  DCHECK(!g_enable_for_testing);
  return std::make_unique<base::AutoReset<bool>>(&g_enable_for_testing, true);
}

// static
void SiteDataCacheFacadeFactory::DisassociateForTesting(Profile* profile) {
  GetInstance()->Disassociate(profile);
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
              .Build()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DependsOn(HistoryServiceFactory::GetInstance());
}

SiteDataCacheFacadeFactory::~SiteDataCacheFacadeFactory() = default;

std::unique_ptr<KeyedService>
SiteDataCacheFacadeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return std::make_unique<SiteDataCacheFacade>(context);
}

bool SiteDataCacheFacadeFactory::ServiceIsCreatedWithBrowserContext() const {
  // It's fine to initialize this service when the browser context
  // gets created so the database will be ready when we need it.
  return true;
}

bool SiteDataCacheFacadeFactory::ServiceIsNULLWhileTesting() const {
  return !g_enable_for_testing;
}

void SiteDataCacheFacadeFactory::OnBeforeFacadeCreated(
    base::PassKey<SiteDataCacheFacade>) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (service_instance_count_ == 0U) {
    CHECK(absl::holds_alternative<absl::monostate>(cache_factory_));
    scoped_refptr<base::SequencedTaskRunner> pm_task_runner =
        performance_manager::PerformanceManager::GetTaskRunner();
    if (base::FeatureList::IsEnabled(features::kRunOnMainThread)) {
      CHECK(pm_task_runner->RunsTasksInCurrentSequence());
      cache_factory_.emplace<SiteDataCacheFactory>();
    } else {
      CHECK(!pm_task_runner->RunsTasksInCurrentSequence());
      cache_factory_.emplace<base::SequenceBound<SiteDataCacheFactory>>(
          std::move(pm_task_runner));
    }
  }
  ++service_instance_count_;
}

void SiteDataCacheFacadeFactory::OnFacadeDestroyed(
    base::PassKey<SiteDataCacheFacade>) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_GT(service_instance_count_, 0U);
  CHECK(!absl::holds_alternative<absl::monostate>(cache_factory_));
  // Destroy the cache factory if there's no more SiteDataCacheFacade needing
  // it.
  if (--service_instance_count_ == 0) {
    cache_factory_.emplace<absl::monostate>();
  }
}

}  // namespace performance_manager
