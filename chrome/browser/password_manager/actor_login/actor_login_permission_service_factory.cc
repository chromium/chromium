// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/actor_login_permission_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"

namespace actor_login {

// static
ActorLoginPermissionService* ActorLoginPermissionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ActorLoginPermissionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ActorLoginPermissionServiceFactory*
ActorLoginPermissionServiceFactory::GetInstance() {
  static base::NoDestructor<ActorLoginPermissionServiceFactory> instance;
  return instance.get();
}

ActorLoginPermissionServiceFactory::ActorLoginPermissionServiceFactory()
    : ProfileKeyedServiceFactory(
          "ActorLoginPermissionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

ActorLoginPermissionServiceFactory::~ActorLoginPermissionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ActorLoginPermissionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ActorLoginPermissionServiceImpl>();
}

}  // namespace actor_login
