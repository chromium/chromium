// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/profiles/profile.h"

// static
MediaEngagementService* MediaEngagementServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaEngagementServiceFactory* MediaEngagementServiceFactory::GetInstance() {
  static base::NoDestructor<MediaEngagementServiceFactory> instance;
  return instance.get();
}

MediaEngagementServiceFactory::MediaEngagementServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaEngagementServiceFactory",
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
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

MediaEngagementServiceFactory::~MediaEngagementServiceFactory() = default;

KeyedService* MediaEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new MediaEngagementService(Profile::FromBrowserContext(context));
}
