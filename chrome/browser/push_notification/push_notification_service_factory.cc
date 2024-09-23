// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_notification/push_notification_service_factory.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_notification/prefs/push_notification_prefs.h"
#include "chrome/browser/push_notification/push_notification_service_desktop_impl.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr char kServiceName[] = "PushNotificationService";

}  // namespace

namespace push_notification {

// static
PushNotificationServiceFactory* PushNotificationServiceFactory::GetInstance() {
  return base::Singleton<PushNotificationServiceFactory>::get();
}

// static
PushNotificationService* PushNotificationServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  // PushNotificationService is currently only implemented for ChromeOS Desktop.
  // If/when iOS and/or Android decide on a Push Notification Service
  // implementation, this CHECK can be revisited.
  CHECK(BUILDFLAG(IS_CHROMEOS_ASH));
  return static_cast<PushNotificationServiceDesktopImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

PushNotificationServiceFactory::PushNotificationServiceFactory()
    : ProfileKeyedServiceFactory(
          kServiceName,
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

PushNotificationServiceFactory::~PushNotificationServiceFactory() = default;

std::unique_ptr<KeyedService>
PushNotificationServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // PushNotificationService is currently only implemented for ChromeOS Desktop.
  // If/when iOS and/or Android decide on a Push Notification Service
  // implementation, this CHECK can be revisited.
  CHECK(BUILDFLAG(IS_CHROMEOS_ASH));
  if (!context) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    return nullptr;
  }

  // Chime is not supported for secondary profiles.
  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile))) {
    return nullptr;
  }

  // Guest/incognito profiles cannot use Chime.
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return nullptr;
  }

  VLOG(1) << __func__ << ": creating PushNotificationService.";

  // Create the service object.
  auto service = std::make_unique<PushNotificationServiceDesktopImpl>(
      Profile::FromBrowserContext(context)->GetPrefs(),
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver(),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory());

  return service;
}

void PushNotificationServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterPushNotificationPrefs(registry);
}

}  // namespace push_notification
