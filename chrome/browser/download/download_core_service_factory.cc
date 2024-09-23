// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_core_service_factory.h"

#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/download/bubble/download_bubble_update_service_factory.h"
#endif  // !BUILDFLAG(IS_ANDROID)

// static
DownloadCoreService* DownloadCoreServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DownloadCoreService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DownloadCoreServiceFactory* DownloadCoreServiceFactory::GetInstance() {
  static base::NoDestructor<DownloadCoreServiceFactory> instance;
  return instance.get();
}

DownloadCoreServiceFactory::DownloadCoreServiceFactory()
    : ProfileKeyedServiceFactory(
          "DownloadCoreService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(DownloadBubbleUpdateServiceFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(OfflineContentAggregatorFactory::GetInstance());
}

DownloadCoreServiceFactory::~DownloadCoreServiceFactory() = default;

KeyedService* DownloadCoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  DownloadCoreService* service =
      new DownloadCoreServiceImpl(static_cast<Profile*>(profile));

  // No need for initialization; initialization can be done on first
  // use of service.

  return service;
}
