// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/nearby_sharing/power_client_chromeos.h"
#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"
#include "components/cross_device/logging/logging.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr char kServiceName[] = "NearbySharingService";
constexpr char kServiceId[] = "NearbySharing";

std::optional<bool>& IsSupportedTesting() {
  static std::optional<bool> is_supported;
  return is_supported;
}

}  // namespace

// static
NearbySharingServiceFactory* NearbySharingServiceFactory::GetInstance() {
  static base::NoDestructor<NearbySharingServiceFactory> instance;
  return instance.get();
}

// static
bool NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
    content::BrowserContext* context) {
  if (IsSupportedTesting().has_value()) {
    return *IsSupportedTesting();
  }

  if (!ash::features::IsCrossDeviceFeatureSuiteAllowed()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(features::kNearbySharing)) {
    return false;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return false;
  }

  // Guest/incognito/signin profiles cannot use Nearby Share.
  if (ash::ProfileHelper::IsSigninProfile(profile) ||
      profile->IsOffTheRecord()) {
    return false;
  }

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

  // Nearby Share is not supported for secondary profiles.
  return ash::ProfileHelper::IsPrimaryProfile(profile);
}

// static
NearbySharingService* NearbySharingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<NearbySharingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

// static
void NearbySharingServiceFactory::
    SetIsNearbyShareSupportedForBrowserContextForTesting(bool is_supported) {
  IsSupportedTesting() = is_supported;
}

NearbySharingServiceFactory::NearbySharingServiceFactory()
    // Nearby Sharing features are disabled in incognito.
    : ProfileKeyedServiceFactory(
          kServiceName,
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ash::nearby::NearbyProcessManagerFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

NearbySharingServiceFactory::~NearbySharingServiceFactory() = default;

std::unique_ptr<KeyedService>
NearbySharingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsNearbyShareSupportedForBrowserContext(context)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  ash::nearby::NearbyProcessManager* process_manager =
      ash::nearby::NearbyProcessManagerFactory::GetForProfile(profile);

  PrefService* pref_service = profile->GetPrefs();
  NotificationDisplayService* notification_display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  auto nearby_connections_manager =
      std::make_unique<NearbyConnectionsManagerImpl>(process_manager,
                                                     kServiceId);

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": creating NearbySharingService for primary profile";

  return std::make_unique<NearbySharingServiceImpl>(
      pref_service, notification_display_service, profile,
      std::move(nearby_connections_manager), process_manager,
      std::make_unique<PowerClientChromeos>(),
      std::make_unique<WifiNetworkConfigurationHandler>());
}

void NearbySharingServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterNearbySharingPrefs(registry);
}

bool NearbySharingServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool NearbySharingServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
