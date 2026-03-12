// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace actor_login {

class ActorLoginPermissionService;

// Singleton that owns all `ActorLoginPermissionService` and associates them
// with Profiles.
class ActorLoginPermissionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ActorLoginPermissionService* GetForProfile(Profile* profile);

  static ActorLoginPermissionServiceFactory* GetInstance();

  ActorLoginPermissionServiceFactory(
      const ActorLoginPermissionServiceFactory&) = delete;
  ActorLoginPermissionServiceFactory& operator=(
      const ActorLoginPermissionServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ActorLoginPermissionServiceFactory>;

  ActorLoginPermissionServiceFactory();
  ~ActorLoginPermissionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_
