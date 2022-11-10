// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_change_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/fileapi/file_change_service.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
FileChangeServiceFactory* FileChangeServiceFactory::GetInstance() {
  static base::NoDestructor<FileChangeServiceFactory> instance;
  return instance.get();
}

FileChangeService* FileChangeServiceFactory::GetService(
    content::BrowserContext* context) {
  return static_cast<FileChangeService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

FileChangeServiceFactory::FileChangeServiceFactory()
    : ProfileKeyedServiceFactory(
          "FileChangeService",
          // Guest sessions are supported and guest OTR profiles are allowed.
          // Don't create the service for OTR profiles outside of guest
          // sessions.
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

FileChangeServiceFactory::~FileChangeServiceFactory() = default;

KeyedService* FileChangeServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  if (profile->IsOffTheRecord())
    CHECK(profile->IsGuestSession());
  return new FileChangeService();
}

}  // namespace ash
