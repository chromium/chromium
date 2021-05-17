// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/nearby_sharing/power_client_chromeos.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr char kServiceName[] = "NearbySharingService";

base::Optional<bool>& IsSupportedTesting() {
  static base::NoDestructor<base::Optional<bool>> is_supported;
  return *is_supported;
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

  if (!chromeos::nearby::NearbyProcessManagerFactory::CanBeLaunchedForProfile(
          profile)) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(features::kNearbySharingChildAccounts) &&
      profile->IsChild()) {
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
    : BrowserContextKeyedServiceFactory(
          kServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(chromeos::nearby::NearbyProcessManagerFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

NearbySharingServiceFactory::~NearbySharingServiceFactory() = default;

KeyedService* NearbySharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!IsNearbyShareSupportedForBrowserContext(context)) {
    NS_LOG(WARNING) << __func__
                    << ": Nearby Share not supported for browser context.";
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  chromeos::nearby::NearbyProcessManager* process_manager =
      chromeos::nearby::NearbyProcessManagerFactory::GetForProfile(profile);

  PrefService* pref_service = profile->GetPrefs();
  NotificationDisplayService* notification_display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  auto nearby_connections_manager =
      std::make_unique<NearbyConnectionsManagerImpl>(process_manager);

  NS_LOG(VERBOSE) << __func__
                  << ": creating NearbySharingService for primary profile";

  return new NearbySharingServiceImpl(
      pref_service, notification_display_service, profile,
      std::move(nearby_connections_manager), process_manager,
      std::make_unique<PowerClientChromeos>());
}

content::BrowserContext* NearbySharingServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Nearby Sharing features are disabled in incognito.
  if (context->IsOffTheRecord())
    return nullptr;

  return context;
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
