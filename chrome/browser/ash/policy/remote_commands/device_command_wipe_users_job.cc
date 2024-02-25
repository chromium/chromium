// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_wipe_users_job.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/system/user_removal_manager.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

// This command has an expiration time this high because the canonical use case
// would be in schools where the admin issues the command at the end of one
// school year, and then in the next school year when a student picks up a
// device from the lot and turns it on for the first time it gets wiped clean
// and ready to be used again. Most schools use the cart model of deployment
// where a given device might not be used in quite some time (eg. device at the
// bottom of the pool would take quite some time to be turned on).
constexpr base::TimeDelta kWipeUsersCommandExpirationTime = base::Days(180);

}  // namespace

DeviceCommandWipeUsersJob::DeviceCommandWipeUsersJob(
    RemoteCommandsService* service)
    : service_(service) {}

DeviceCommandWipeUsersJob::~DeviceCommandWipeUsersJob() = default;

enterprise_management::RemoteCommand_Type DeviceCommandWipeUsersJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_WIPE_USERS;
}

bool DeviceCommandWipeUsersJob::IsExpired(base::TimeTicks now) {
  return now > issued_time() + kWipeUsersCommandExpirationTime;
}

void DeviceCommandWipeUsersJob::RunImpl(CallbackWithResult result_callback) {
  // Set callback which gets called after command is ACKd to the server. We want
  // to log out only after the server got the ACK, otherwise we could log out
  // before ACKing and then the server would never get the ACK.
  service_->SetOnCommandAckedCallback(
      base::BindOnce(&ash::user_removal_manager::LogOut));

  // Initiate the user removal process. Once the first part is done, the passed
  // callback gets called and signals that the command was successfully received
  // and will be executed.
  ash::user_removal_manager::InitiateUserRemoval(base::BindOnce(
      std::move(result_callback), ResultType::kSuccess, std::nullopt));
}

}  // namespace policy
