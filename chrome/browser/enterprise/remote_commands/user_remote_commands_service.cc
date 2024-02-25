// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

namespace enterprise_commands {

UserRemoteCommandsService::UserRemoteCommandsService(Profile* profile)
    : policy::UserRemoteCommandsServiceBase(
          profile->GetUserCloudPolicyManager()->core()),
      profile_(profile) {}
UserRemoteCommandsService::~UserRemoteCommandsService() = default;

invalidation::ProfileInvalidationProvider*
UserRemoteCommandsService::GetInvalidationProvider() {
  return invalidation::ProfileInvalidationProviderFactory::GetForProfile(
      profile_);
}

std::unique_ptr<policy::RemoteCommandsFactory>
UserRemoteCommandsService::GetFactory() {
  return std::make_unique<UserRemoteCommandsFactory>(profile_);
}

}  // namespace enterprise_commands
