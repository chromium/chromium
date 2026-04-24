// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"
#include "chrome/browser/enterprise/remote_commands/extension_update_check_job.h"

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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    case enterprise_management::RemoteCommand_Type_EXTENSION_UPDATE_CHECK:
      return std::make_unique<ExtensionUpdateCheckJob>(profile_);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    default:
      NOTREACHED() << "Received an unsupported remote command type: " << type;
  }
}
}  // namespace enterprise_commands
