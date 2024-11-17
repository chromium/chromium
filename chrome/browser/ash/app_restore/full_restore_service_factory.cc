// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace ash::full_restore {

// static
bool FullRestoreServiceFactory::IsFullRestoreAvailableForProfile(
    const Profile* profile) {
  if (IsRunningInForcedAppMode() || DemoSession::IsDeviceInDemoMode()) {
    return false;
  }

  // No service for non-regular user profile, or ephemeral user profile, system
  // profile.
  if (!profile || profile->IsSystemProfile() ||
      !ProfileHelper::IsUserProfile(profile) ||
      ProfileHelper::IsEphemeralUserProfile(profile)) {
    return false;
  }

  return profile->GetPrefs()->GetBoolean(kRestoreAppsEnabled);
}

// static
FullRestoreServiceFactory* FullRestoreServiceFactory::GetInstance() {
  static base::NoDestructor<FullRestoreServiceFactory> instance;
  return instance.get();
}

// static
FullRestoreService* FullRestoreServiceFactory::GetForProfile(Profile* profile) {
  TRACE_EVENT0("ui", "FullRestoreServiceFactory::GetForProfile");
  return static_cast<FullRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FullRestoreServiceFactory::FullRestoreServiceFactory()
    : ProfileKeyedServiceFactory("FullRestoreService",
                                 ProfileSelections::Builder()
                                     .WithGuest(ProfileSelection::kOriginalOnly)
                                     .WithSystem(ProfileSelection::kNone)
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

FullRestoreServiceFactory::~FullRestoreServiceFactory() = default;

std::unique_ptr<KeyedService>
FullRestoreServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!IsFullRestoreAvailableForProfile(profile))
    return nullptr;

  return std::make_unique<FullRestoreService>(profile);
}

}  // namespace ash::full_restore
