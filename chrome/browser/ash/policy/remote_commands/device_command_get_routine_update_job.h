// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_GET_ROUTINE_UPDATE_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_GET_ROUTINE_UPDATE_JOB_H_

#include <cstdint>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

// This class implements a RemoteCommandJob that sends a command to an existing
// diagnostic routine on the platform and returns an update on that routine. The
// RemoteCommandsQueue owns all instances of this class.
class DeviceCommandGetRoutineUpdateJob : public RemoteCommandJob {
 public:
  DeviceCommandGetRoutineUpdateJob();
  DeviceCommandGetRoutineUpdateJob(const DeviceCommandGetRoutineUpdateJob&) =
      delete;
  DeviceCommandGetRoutineUpdateJob& operator=(
      const DeviceCommandGetRoutineUpdateJob&) = delete;
  ~DeviceCommandGetRoutineUpdateJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult result_callback) override;

  void OnCrosHealthdResponseReceived(
      CallbackWithResult result_callback,
      ash::cros_healthd::mojom::RoutineUpdatePtr update);

  // The ID of the routine to send the command to.
  int32_t routine_id_;
  // Which command to send to the routine.
  ash::cros_healthd::mojom::DiagnosticRoutineCommandEnum command_;
  // Whether or not output should be included in the response to the command.
  bool include_output_;

  base::WeakPtrFactory<DeviceCommandGetRoutineUpdateJob> weak_ptr_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_GET_ROUTINE_UPDATE_JOB_H_
