// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_factory.h"

#include "base/notreached.h"
#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"

namespace enterprise_commands {

UserRemoteCommandsFactory::UserRemoteCommandsFactory(Profile* profile)
    : profile_(profile) {}
UserRemoteCommandsFactory::~UserRemoteCommandsFactory() = default;

std::unique_ptr<policy::RemoteCommandJob>
UserRemoteCommandsFactory::BuildJobForType(
    enterprise_management::RemoteCommand_Type type,
    policy::RemoteCommandsService* service) {
  switch (type) {
    case enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA:
      return std::make_unique<ClearBrowsingDataJob>(profile_);
    default:
      NOTREACHED_IN_MIGRATION()
          << "Received an unsupported remote command type: " << type;
      return nullptr;
  }
}
}  // namespace enterprise_commands
