// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

PhotosService* PhotosServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PhotosService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PhotosServiceFactory* PhotosServiceFactory::GetInstance() {
  static base::NoDestructor<PhotosServiceFactory> instance;
  return instance.get();
}

PhotosServiceFactory::PhotosServiceFactory()
    : ProfileKeyedServiceFactory(
          "PhotosService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PhotosServiceFactory::~PhotosServiceFactory() = default;

std::unique_ptr<KeyedService>
PhotosServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto url_loader_factory = context->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PhotosService>(
      url_loader_factory, IdentityManagerFactory::GetForProfile(profile),
      profile->GetPrefs());
}
