// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager_impl.h"
#include "chrome/browser/nearby_sharing/nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/nearby_sharing/power_client_chromeos.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/nearby_sharing/power_client.h"
#endif  // defined(OS_CHROMEOS)

namespace {

constexpr char kServiceName[] = "NearbySharingService";

}  // namespace

// static
NearbySharingServiceFactory* NearbySharingServiceFactory::GetInstance() {
  return base::Singleton<NearbySharingServiceFactory>::get();
}

// static
NearbySharingService* NearbySharingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<NearbySharingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

NearbySharingServiceFactory::NearbySharingServiceFactory()
    : BrowserContextKeyedServiceFactory(
          kServiceName,
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
}

NearbySharingServiceFactory::~NearbySharingServiceFactory() = default;

KeyedService* NearbySharingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kNearbySharing)) {
    NS_LOG(VERBOSE) << __func__
                    << ": Nearby Sharing feature flag is not enabled.";
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  NearbyProcessManager& process_manager = NearbyProcessManager::GetInstance();

#if defined(OS_CHROMEOS)
  // On ChromeOS we will only support the active profile.
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    NS_LOG(VERBOSE)
        << __func__
        << ": Nearby Sharing service is skipping non-primary profile: "
        << profile->GetProfileUserName();
    return nullptr;
  }
  NS_LOG(VERBOSE) << __func__
                  << "Nearby Sharing service is forcing active profile: "
                  << profile->GetProfileUserName();
  // Force active profile for ChromeOS for now.
  process_manager.SetActiveProfile(profile);
#endif

  PrefService* pref_service = profile->GetPrefs();
  NotificationDisplayService* notification_display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);

  auto nearby_connections_manager =
      std::make_unique<NearbyConnectionsManagerImpl>(&process_manager, profile);

  NS_LOG(VERBOSE) << __func__ << ": creating NearbySharingService for profile: "
                  << profile->GetProfileUserName();

  return new NearbySharingServiceImpl(
      pref_service, notification_display_service, profile,
      std::move(nearby_connections_manager), &process_manager,
#if defined(OS_CHROMEOS)
      std::make_unique<PowerClientChromeos>());
#else   // !defined(OS_CHROMEOS)
      std::make_unique<PowerClient>());
#endif  // defined(OS_CHROMEOS)
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
