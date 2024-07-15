// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_notifier.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_service.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_store.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {
constexpr char kServiceName[] = "AppsControlsService";
}

namespace ash::on_device_controls {

// static
AppControlsServiceFactory* AppControlsServiceFactory::GetInstance() {
  static base::NoDestructor<AppControlsServiceFactory> instance;
  return instance.get();
}

// static
bool AppControlsServiceFactory::IsOnDeviceAppControlsAvailable(
    content::BrowserContext* context) {
  // On device apps parental controls is only available for unmanaged consumer
  // users.
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  if (profile->GetProfilePolicyConnector()->IsManaged() || profile->IsChild()) {
    return false;
  }

  const std::string region = on_device_controls::GetDeviceRegionCode();
  return (features::IsOnDeviceAppControlsEnabled() &&
          on_device_controls::IsOnDeviceControlsRegion(region)) ||
         features::ForceOnDeviceAppControlsForAllRegions();
}

// static
AppControlsService* AppControlsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppControlsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

AppControlsServiceFactory::AppControlsServiceFactory()
    : ProfileKeyedServiceFactory(
          kServiceName,
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

AppControlsServiceFactory::~AppControlsServiceFactory() = default;

std::unique_ptr<KeyedService>
AppControlsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AppControlsService>();
}

void AppControlsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AppControlsService::RegisterProfilePrefs(registry);
  AppControlsNotifier::RegisterProfilePrefs(registry);
  BlockedAppStore::RegisterProfilePrefs(registry);
}

}  // namespace ash::on_device_controls
