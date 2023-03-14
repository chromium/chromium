// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/nearby_sharing/power_client_chromeos.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr char kServiceName[] = "NearbySharingService";
constexpr char kServiceId[] = "NearbySharing";

absl::optional<bool>& IsSupportedTesting() {
  static absl::optional<bool> is_supported;
  return is_supported;
}

}  // namespace

// static
NearbySharingServiceFactory* NearbySharingServiceFactory::GetInstance() {
  return base::Singleton<NearbySharingServiceFactory>::get();
}

// static
bool NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
    content::BrowserContext* context) {
  if (IsSupportedTesting().has_value())
    return *IsSupportedTesting();

  if (!base::FeatureList::IsEnabled(features::kNearbySharing))
    return false;

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return false;

  if (!ash::nearby::NearbyProcessManagerFactory::CanBeLaunchedForProfile(
          profile)) {
    return false;
  }

  return true;
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
    : ProfileKeyedServiceFactory(kServiceName) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ash::nearby::NearbyProcessManagerFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

NearbySharingServiceFactory::~NearbySharingServiceFactory() = default;

KeyedService* NearbySharingServiceFactory::BuildServiceInstanceFor(
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

  NS_LOG(VERBOSE) << __func__
                  << ": creating NearbySharingService for primary profile";

  return new NearbySharingServiceImpl(
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
