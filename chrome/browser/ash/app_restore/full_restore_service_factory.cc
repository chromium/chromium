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
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash::full_restore {

// static
bool FullRestoreServiceFactory::IsFullRestoreAvailableForUser(
    const user_manager::User& user) {
  if (IsRunningInForcedAppMode() || ash::demo_mode::IsDeviceInDemoMode()) {
    return false;
  }

  // No service for an ephemeral user.
  if (user_manager::UserManager::Get()->IsEphemeralUser(&user)) {
    return false;
  }

  // The prefs are gone once the user's profile has been destroyed, and are not
  // there yet before it has been created.
  const PrefService* const prefs = user.GetProfilePrefs();
  if (!prefs) {
    return false;
  }

  // Floating Workspace is an enterprise feature which restores user's desks
  // across devices. If it's enabled, it provides an alternative restore
  // behavior independent of Full Restore.
  if (floating_workspace_util::IsFloatingWorkspaceEnabled(*prefs)) {
    return false;
  }

  return prefs->GetBoolean(kRestoreAppsEnabled);
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
    : ProfileKeyedServiceFactory(
          "FullRestoreService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

FullRestoreServiceFactory::~FullRestoreServiceFactory() = default;

std::unique_ptr<KeyedService>
FullRestoreServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  const user_manager::User* const user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(context);
  if (!user || !IsFullRestoreAvailableForUser(*user)) {
    return nullptr;
  }

  return std::make_unique<FullRestoreService>(
      Profile::FromBrowserContext(context));
}

}  // namespace ash::full_restore
