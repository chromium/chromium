// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace apps {

DeviceInfoManager* DeviceInfoManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<DeviceInfoManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DeviceInfoManagerFactory* DeviceInfoManagerFactory::GetInstance() {
  static base::NoDestructor<DeviceInfoManagerFactory> instance;
  return instance.get();
}

DeviceInfoManagerFactory::DeviceInfoManagerFactory()
    : ProfileKeyedServiceFactory(
          "DeviceInfoManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

DeviceInfoManagerFactory::~DeviceInfoManagerFactory() = default;

std::unique_ptr<KeyedService>
DeviceInfoManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return base::WrapUnique<DeviceInfoManager>(
      new DeviceInfoManager(Profile::FromBrowserContext(context)));
}

}  // namespace apps
