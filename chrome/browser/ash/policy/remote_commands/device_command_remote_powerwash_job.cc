// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_remote_powerwash_job.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

namespace {

// Expiration time for the command is this high because this command is supposed
// to be a security feature where the device gets wiped even if it's turned on
// again only after several years of being powered off.
constexpr base::TimeDelta kRemotePowerwashCommandExpirationTime =
    base::Days(5 * 365);  // 5 years.

// The time that we wait for the server to get the ACK, if that passes we
// immediately start the powerwash process.
constexpr base::TimeDelta kFailsafeTimerTimeout = base::Seconds(10);

void StartPowerwash(enterprise_management::SignedData signed_command) {
  ash::SessionManagerClient::Get()->StartRemoteDeviceWipe(signed_command);
}

}  // namespace

DeviceCommandRemotePowerwashJob::DeviceCommandRemotePowerwashJob(
    RemoteCommandsService* service)
    : service_(service) {}

DeviceCommandRemotePowerwashJob::~DeviceCommandRemotePowerwashJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandRemotePowerwashJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_DEVICE_REMOTE_POWERWASH;
}

bool DeviceCommandRemotePowerwashJob::IsExpired(base::TimeTicks now) {
  return now > issued_time() + kRemotePowerwashCommandExpirationTime;
}

void DeviceCommandRemotePowerwashJob::RunImpl(
    CallbackWithResult result_callback) {
  // Set callback which gets called after command is ACKd to the server. We want
  // to start the powerwash process only after the server got the ACK, otherwise
  // we could reboot before ACKing and then the server would never get the ACK.
  service_->SetOnCommandAckedCallback(
      base::BindOnce(&StartPowerwash, signed_command()));

  // Also set a failsafe timer that starts the powerwash so a faulty network
  // connection doesn't prevent the powerwash from happening.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&StartPowerwash, signed_command()),
      kFailsafeTimerTimeout);

  // Ack the command.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback),
                                ResultType::kSuccess, std::nullopt));
}

}  // namespace policy
