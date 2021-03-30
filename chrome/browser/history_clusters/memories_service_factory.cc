// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_clusters/memories_service_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history_clusters/core/memories_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/storage_partition.h"

// static
memories::MemoriesService* MemoriesServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<memories::MemoriesService*>(
      GetInstance().GetServiceForBrowserContext(browser_context, true));
}

// static
MemoriesServiceFactory& MemoriesServiceFactory::GetInstance() {
  static base::NoDestructor<MemoriesServiceFactory> instance;
  return *instance;
}

MemoriesServiceFactory::MemoriesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MemoriesService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

MemoriesServiceFactory::~MemoriesServiceFactory() = default;

KeyedService* MemoriesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new memories::MemoriesService(history_service, url_loader_factory);
}

content::BrowserContext* MemoriesServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Give incognito its own isolated service.
  return context;
}
