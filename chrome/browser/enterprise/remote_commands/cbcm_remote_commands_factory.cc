// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/cbcm_remote_commands_factory.h"

#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace enterprise_commands {

std::unique_ptr<policy::RemoteCommandJob>
CBCMRemoteCommandsFactory::BuildJobForType(
    enterprise_management::RemoteCommand_Type type,
    policy::RemoteCommandsService* service) {
  switch (type) {
    case enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA:
      return std::make_unique<ClearBrowsingDataJob>(
          g_browser_process->profile_manager());
    default:
      NOTREACHED() << "Received an unsupported remote command type: " << type;
      return nullptr;
  }
}

}  // namespace enterprise_commands
