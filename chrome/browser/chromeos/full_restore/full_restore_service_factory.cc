// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/full_restore/full_restore_service_factory.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/full_restore/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {
namespace full_restore {

// static
bool FullRestoreServiceFactory::IsFullRestoreAvailableForProfile(
    const Profile* profile) {
  if (!::full_restore::features::IsFullRestoreEnabled())
    return false;

  if (chrome::IsRunningInForcedAppMode())
    return false;

  // No service for non-regular user profile, or ephemeral user profile, system
  // profile.
  if (!profile || profile->IsSystemProfile() ||
      !ProfileHelper::IsRegularProfile(profile) ||
      ProfileHelper::IsEphemeralUserProfile(profile)) {
    return false;
  }

  return true;
}

// static
FullRestoreServiceFactory* FullRestoreServiceFactory::GetInstance() {
  static base::NoDestructor<FullRestoreServiceFactory> instance;
  return instance.get();
}

// static
FullRestoreService* FullRestoreServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FullRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FullRestoreServiceFactory::FullRestoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "FullRestoreService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

FullRestoreServiceFactory::~FullRestoreServiceFactory() = default;

KeyedService* FullRestoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!IsFullRestoreAvailableForProfile(Profile::FromBrowserContext(context)))
    return nullptr;

  return new FullRestoreService(Profile::FromBrowserContext(context));
}

}  // namespace full_restore
}  // namespace chromeos
