// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_factory.h"

#include "chrome/browser/content_index/content_index_provider_impl.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"

// static
ContentIndexProviderImpl* ContentIndexProviderFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContentIndexProviderImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContentIndexProviderFactory* ContentIndexProviderFactory::GetInstance() {
  static base::NoDestructor<ContentIndexProviderFactory> instance;
  return instance.get();
}

ContentIndexProviderFactory::ContentIndexProviderFactory()
    : ProfileKeyedServiceFactory(
          "ContentIndexProvider",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(OfflineContentAggregatorFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
  DependsOn(site_engagement::SiteEngagementServiceFactory::GetInstance());
}

ContentIndexProviderFactory::~ContentIndexProviderFactory() = default;

std::unique_ptr<KeyedService>
ContentIndexProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ContentIndexProviderImpl>(
      Profile::FromBrowserContext(context));
}
