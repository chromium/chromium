// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_permission_cleaning_service_factory.h"

#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_permission_cleaning_service.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace actor_login {

// static
ActorLoginPermissionCleaningService*
ActorLoginPermissionCleaningServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ActorLoginPermissionCleaningService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ActorLoginPermissionCleaningServiceFactory*
ActorLoginPermissionCleaningServiceFactory::GetInstance() {
  static base::NoDestructor<ActorLoginPermissionCleaningServiceFactory>
      instance;
  return instance.get();
}

ActorLoginPermissionCleaningServiceFactory::
    ActorLoginPermissionCleaningServiceFactory()
    : ProfileKeyedServiceFactory(
          "ActorLoginDuplicatePermissionCleaner",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(ActorLoginPermissionServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
}

ActorLoginPermissionCleaningServiceFactory::
    ~ActorLoginPermissionCleaningServiceFactory() = default;

std::unique_ptr<KeyedService> ActorLoginPermissionCleaningServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ActorLoginPermissionCleaningService>(
      ActorLoginPermissionServiceFactory::GetForProfile(profile),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS));
}

}  // namespace actor_login
