// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/remote_apps/remote_apps_manager_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

// static
RemoteAppsManager* RemoteAppsManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<RemoteAppsManager*>(
      RemoteAppsManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
RemoteAppsManagerFactory* RemoteAppsManagerFactory::GetInstance() {
  static base::NoDestructor<RemoteAppsManagerFactory> instance;
  return instance.get();
}

RemoteAppsManagerFactory::RemoteAppsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "RemoteAppsManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(app_list::AppListSyncableServiceFactory::GetInstance());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

RemoteAppsManagerFactory::~RemoteAppsManagerFactory() = default;

KeyedService* RemoteAppsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  chromeos::ProfileHelper* profile_helper = chromeos::ProfileHelper::Get();
  if (!profile_helper)
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  user_manager::User* user = profile_helper->GetUserByProfile(profile);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return nullptr;

  return new RemoteAppsManager(profile);
}

content::BrowserContext* RemoteAppsManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile || profile->IsSystemProfile() ||
      !chromeos::ProfileHelper::IsRegularProfile(profile)) {
    return nullptr;
  }

  return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
}

bool RemoteAppsManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chromeos
