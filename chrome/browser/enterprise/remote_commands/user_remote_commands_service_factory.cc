// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/cloud/user_fm_registration_token_uploader_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace enterprise_commands {

// static
UserRemoteCommandsServiceFactory*
UserRemoteCommandsServiceFactory::GetInstance() {
  static base::NoDestructor<UserRemoteCommandsServiceFactory> instance;
  return instance.get();
}

// static
UserRemoteCommandsService* UserRemoteCommandsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserRemoteCommandsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}
std::unique_ptr<KeyedService>
UserRemoteCommandsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // UserCloudPolicyManager doesn't exist for Lacros main profile or in some
  // test environments. In those cases, we skip the creation of KeyedService
  // here.
  if (!profile->GetUserCloudPolicyManager()) {
    return nullptr;
  }
  return std::make_unique<UserRemoteCommandsService>(profile);
}

UserRemoteCommandsServiceFactory::UserRemoteCommandsServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserRemoteCommands",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(invalidation::ProfileInvalidationProviderFactory::GetInstance());
  DependsOn(policy::UserFmRegistrationTokenUploaderFactory::GetInstance());
}
UserRemoteCommandsServiceFactory::~UserRemoteCommandsServiceFactory() = default;

}  // namespace enterprise_commands
