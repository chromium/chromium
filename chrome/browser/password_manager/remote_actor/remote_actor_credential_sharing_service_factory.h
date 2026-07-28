// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace password_manager {

class RemoteActorCredentialSharingService;

class RemoteActorCredentialSharingServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static RemoteActorCredentialSharingService* GetForProfile(Profile* profile);
  static RemoteActorCredentialSharingServiceFactory* GetInstance();

  RemoteActorCredentialSharingServiceFactory(
      const RemoteActorCredentialSharingServiceFactory&) = delete;
  RemoteActorCredentialSharingServiceFactory& operator=(
      const RemoteActorCredentialSharingServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<RemoteActorCredentialSharingServiceFactory>;

  RemoteActorCredentialSharingServiceFactory();
  ~RemoteActorCredentialSharingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_REMOTE_ACTOR_REMOTE_ACTOR_CREDENTIAL_SHARING_SERVICE_FACTORY_H_
