// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/download/bubble/download_bubble_update_service.h"
#include "chrome/browser/download/offline_item_model_manager_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

// static
DownloadBubbleUpdateServiceFactory*
DownloadBubbleUpdateServiceFactory::GetInstance() {
  static base::NoDestructor<DownloadBubbleUpdateServiceFactory> instance;
  return instance.get();
}

// static
DownloadBubbleUpdateService* DownloadBubbleUpdateServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DownloadBubbleUpdateService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

DownloadBubbleUpdateServiceFactory::DownloadBubbleUpdateServiceFactory()
    : ProfileKeyedServiceFactory(
          "DownloadBubbleUpdateService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(OfflineContentAggregatorFactory::GetInstance());
  DependsOn(OfflineItemModelManagerFactory::GetInstance());
}

DownloadBubbleUpdateServiceFactory::~DownloadBubbleUpdateServiceFactory() =
    default;

std::unique_ptr<KeyedService>
DownloadBubbleUpdateServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<DownloadBubbleUpdateService>(
      Profile::FromBrowserContext(context));
}
