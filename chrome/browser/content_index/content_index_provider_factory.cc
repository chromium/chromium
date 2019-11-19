// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_factory.h"

#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ContentIndexProviderImpl* ContentIndexProviderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContentIndexProviderImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContentIndexProviderFactory* ContentIndexProviderFactory::GetInstance() {
  return base::Singleton<ContentIndexProviderFactory>::get();
}

ContentIndexProviderFactory::ContentIndexProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "ContentIndexProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(OfflineContentAggregatorFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
  DependsOn(SiteEngagementServiceFactory::GetInstance());
}

ContentIndexProviderFactory::~ContentIndexProviderFactory() = default;

KeyedService* ContentIndexProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ContentIndexProviderImpl(Profile::FromBrowserContext(context));
}

content::BrowserContext* ContentIndexProviderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
