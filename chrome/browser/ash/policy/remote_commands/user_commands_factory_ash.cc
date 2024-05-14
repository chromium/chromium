// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/user_commands_factory_ash.h"

#include "base/notreached.h"
#include "chrome/browser/ash/policy/remote_commands/user_command_arc_job.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

UserCommandsFactoryAsh::UserCommandsFactoryAsh(Profile* profile)
    : profile_(profile) {}

UserCommandsFactoryAsh::~UserCommandsFactoryAsh() = default;

std::unique_ptr<RemoteCommandJob> UserCommandsFactoryAsh::BuildJobForType(
    em::RemoteCommand_Type type,
    RemoteCommandsService* service) {
  switch (type) {
    case em::RemoteCommand_Type_USER_ARC_COMMAND:
      return std::make_unique<UserCommandArcJob>(profile_);
    default:
      // Other types of commands should be sent to DeviceCommandsFactoryAsh
      // instead of here.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

}  // namespace policy
