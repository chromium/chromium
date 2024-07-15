// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

// static
StorageAccessAPIServiceImpl*
StorageAccessAPIServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<StorageAccessAPIServiceImpl*>(
      StorageAccessAPIServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
StorageAccessAPIServiceFactory* StorageAccessAPIServiceFactory::GetInstance() {
  static base::NoDestructor<StorageAccessAPIServiceFactory> instance;
  return instance.get();
}

StorageAccessAPIServiceFactory::StorageAccessAPIServiceFactory()
    : ProfileKeyedServiceFactory(
          "StorageAccessAPIService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

StorageAccessAPIServiceFactory::~StorageAccessAPIServiceFactory() = default;

std::unique_ptr<KeyedService>
StorageAccessAPIServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<StorageAccessAPIServiceImpl>(context);
}
