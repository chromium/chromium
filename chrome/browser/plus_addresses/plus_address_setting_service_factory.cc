// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "components/plus_addresses/settings/plus_address_setting_service_impl.h"
#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"
#include "components/sync/model/data_type_store_service.h"

// static
PlusAddressSettingServiceFactory*
PlusAddressSettingServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressSettingServiceFactory> instance;
  return instance.get();
}

// static
plus_addresses::PlusAddressSettingService*
PlusAddressSettingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<plus_addresses::PlusAddressSettingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

PlusAddressSettingServiceFactory::PlusAddressSettingServiceFactory()
    : ProfileKeyedServiceFactory(
          "PlusAddressSettingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PlusAddressSettingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<plus_addresses::PlusAddressSettingServiceImpl>(
      plus_addresses::PlusAddressSettingSyncBridge::CreateBridge(
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory()));
}
