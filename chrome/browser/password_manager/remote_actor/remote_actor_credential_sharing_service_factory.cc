// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/storage_partition.h"

namespace password_manager {

// static
RemoteActorCredentialSharingService*
RemoteActorCredentialSharingServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<RemoteActorCredentialSharingService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
RemoteActorCredentialSharingServiceFactory*
RemoteActorCredentialSharingServiceFactory::GetInstance() {
  static base::NoDestructor<RemoteActorCredentialSharingServiceFactory>
      instance;
  return instance.get();
}

RemoteActorCredentialSharingServiceFactory::
    RemoteActorCredentialSharingServiceFactory()
    : ProfileKeyedServiceFactory("RemoteActorCredentialSharingService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

RemoteActorCredentialSharingServiceFactory::
    ~RemoteActorCredentialSharingServiceFactory() = default;

std::unique_ptr<KeyedService> RemoteActorCredentialSharingServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  if (!features::RemoteActorCredentialSharingEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<RemoteActorCredentialSharingServiceImpl>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

}  // namespace password_manager
