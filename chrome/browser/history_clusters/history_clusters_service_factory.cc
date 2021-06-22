// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
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
    : BrowserContextKeyedServiceFactory(
          "HistoryClustersService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
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
  return new history_clusters::HistoryClustersService(history_service,
                                                      url_loader_factory);
}

content::BrowserContext* HistoryClustersServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Give incognito its own isolated service.
  return context;
}
