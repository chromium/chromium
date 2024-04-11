// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_apps_parental_controls_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_apps_parental_controls_service.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"

namespace {
constexpr char kServiceName[] = "OnDeviceAppsParentalControlsService";
}

namespace ash {

// static
OnDeviceAppsParentalControlsServiceFactory*
OnDeviceAppsParentalControlsServiceFactory::GetInstance() {
  static base::NoDestructor<OnDeviceAppsParentalControlsServiceFactory>
      instance;
  return instance.get();
}

// static
bool OnDeviceAppsParentalControlsServiceFactory::
    IsOnDeviceAppsParentalControlsAvailable(content::BrowserContext* context) {
  // On device apps parental controls is only available for unmanaged consumer
  // users.
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  if (profile->GetProfilePolicyConnector()->IsManaged() || profile->IsChild()) {
    return false;
  }

  if (!features::IsAdditionalOnDeviceAppsParentalControlsEnabled()) {
    return false;
  }

  const std::string region = on_device_controls::GetDeviceRegionCode();
  return on_device_controls::IsOnDeviceControlsRegion(region) ||
         features::
             IsForceAdditionalOnDeviceAppsParentalControlsAllRegionsEnabled();
}

// static
OnDeviceAppsParentalControlsService*
OnDeviceAppsParentalControlsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OnDeviceAppsParentalControlsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

OnDeviceAppsParentalControlsServiceFactory::
    OnDeviceAppsParentalControlsServiceFactory()
    : ProfileKeyedServiceFactory(kServiceName) {}

OnDeviceAppsParentalControlsServiceFactory::
    ~OnDeviceAppsParentalControlsServiceFactory() = default;

std::unique_ptr<KeyedService> OnDeviceAppsParentalControlsServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<OnDeviceAppsParentalControlsService>();
}

}  // namespace ash
