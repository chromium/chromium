// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/storage_partition.h"

namespace {

std::unique_ptr<KeyedService> BuildService(content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  // The clusters service can't function without a HistoryService. This happens
  // in some unit tests.
  if (!history_service) {
    return nullptr;
  }

  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  return std::make_unique<history_clusters::HistoryClustersService>(
      g_browser_process->GetApplicationLocale(), history_service,
      url_loader_factory, site_engagement::SiteEngagementService::Get(profile),
      TemplateURLServiceFactory::GetForProfile(profile),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      profile->GetPrefs());
}

}  // namespace

// static
history_clusters::HistoryClustersService*
HistoryClustersServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<history_clusters::HistoryClustersService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
HistoryClustersServiceFactory* HistoryClustersServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryClustersServiceFactory> instance;
  return instance.get();
}

HistoryClustersServiceFactory::HistoryClustersServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistoryClustersService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

HistoryClustersServiceFactory::~HistoryClustersServiceFactory() = default;

// static
BrowserContextKeyedServiceFactory::TestingFactory
HistoryClustersServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildService);
}

std::unique_ptr<KeyedService>
HistoryClustersServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildService(context);
}

// static
void HistoryClustersServiceFactory::EnsureFactoryBuilt() {
  GetInstance();
}
