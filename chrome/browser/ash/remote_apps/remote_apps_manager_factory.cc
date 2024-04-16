// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"

namespace ash {

// static
RemoteAppsManager* RemoteAppsManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<RemoteAppsManager*>(
      RemoteAppsManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
RemoteAppsManagerFactory* RemoteAppsManagerFactory::GetInstance() {
  // TODO(crbug.com/40205142): Restore use of base::NoDestructor when
  // it no longer causes unit_test failures.
  static base::NoDestructor<RemoteAppsManagerFactory> instance;
  return instance.get();
}

RemoteAppsManagerFactory::RemoteAppsManagerFactory()
    : ProfileKeyedServiceFactory("RemoteAppsManager",
                                 ProfileSelections::Builder()
                                     .WithGuest(ProfileSelection::kOriginalOnly)
                                     .WithSystem(ProfileSelection::kNone)
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {
  DependsOn(app_list::AppListSyncableServiceFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
  DependsOn(extensions::EventRouterFactory::GetInstance());
}

RemoteAppsManagerFactory::~RemoteAppsManagerFactory() = default;

std::unique_ptr<KeyedService>
RemoteAppsManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  ProfileHelper* profile_helper = ProfileHelper::Get();
  if (!profile_helper)
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  user_manager::User* user = profile_helper->GetUserByProfile(profile);
  if (!user || (user->GetType() != user_manager::UserType::kPublicAccount &&
                user->GetType() != user_manager::UserType::kRegular)) {
    return nullptr;
  }

  return std::make_unique<RemoteAppsManager>(profile);
}

bool RemoteAppsManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool RemoteAppsManagerFactory::ServiceIsNULLWhileTesting() const {
  // `RemoteAppsManager` depends on `AppListSyncableService` to be useful,
  // meaning that it's availability for testing also depends on
  // `AppListSyncableService`'s service availability.
  return !app_list::AppListSyncableServiceFactory::IsUsedInTesting();
}

}  // namespace ash
