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
#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/storage_partition.h"

// static
history_clusters::HistoryClustersService*
HistoryClustersServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<history_clusters::HistoryClustersService*>(
      GetInstance().GetServiceForBrowserContext(browser_context, true));
}

// static
HistoryClustersServiceFactory& HistoryClustersServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryClustersServiceFactory> instance;
  return *instance;
}

HistoryClustersServiceFactory::HistoryClustersServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistoryClustersService",
          // Give incognito its own isolated service.
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PageContentAnnotationsServiceFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

HistoryClustersServiceFactory::~HistoryClustersServiceFactory() = default;

KeyedService* HistoryClustersServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  // The clusters service can't function without a HistoryService. This happens
  // in some unit tests.
  if (!history_service)
    return nullptr;

  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  return new history_clusters::HistoryClustersService(
      g_browser_process->GetApplicationLocale(), history_service,
      PageContentAnnotationsServiceFactory::GetForProfile(profile),
      url_loader_factory, site_engagement::SiteEngagementService::Get(profile),
      TemplateURLServiceFactory::GetForProfile(profile),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      profile->GetPrefs());
}
