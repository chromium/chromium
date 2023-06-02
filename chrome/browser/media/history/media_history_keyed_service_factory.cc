// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_keyed_service_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace media_history {

// static
MediaHistoryKeyedService* MediaHistoryKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaHistoryKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaHistoryKeyedServiceFactory*
MediaHistoryKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<MediaHistoryKeyedServiceFactory> instance;
  return instance.get();
}

MediaHistoryKeyedServiceFactory::MediaHistoryKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaHistoryKeyedService",
          // Enable incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}

MediaHistoryKeyedServiceFactory::~MediaHistoryKeyedServiceFactory() = default;

bool MediaHistoryKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* MediaHistoryKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new MediaHistoryKeyedService(Profile::FromBrowserContext(context));
}

}  // namespace media_history
