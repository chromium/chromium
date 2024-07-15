// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"

namespace sharing_hub {

// static
SharingHubService* SharingHubServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SharingHubService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SharingHubServiceFactory* SharingHubServiceFactory::GetInstance() {
  static base::NoDestructor<SharingHubServiceFactory> instance;
  return instance.get();
}

SharingHubServiceFactory::SharingHubServiceFactory()
    : ProfileKeyedServiceFactory(
          "SharingHubService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

SharingHubServiceFactory::~SharingHubServiceFactory() = default;

std::unique_ptr<KeyedService>
SharingHubServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SharingHubService>(context);
}

}  // namespace sharing_hub
