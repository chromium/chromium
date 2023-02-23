// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RUN_ROUTINE_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RUN_ROUTINE_JOB_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

// This class implements a RemoteCommandJob that runs a diagnostic routine on
// the platform. The RemoteCommandsQueue owns all instances of this class.
class DeviceCommandRunRoutineJob : public RemoteCommandJob {
 public:
  // String constant identifying the parameter field for the video conferencing
  // routine. Note that stunServerHostname is an optional parameter.
  static constexpr char kStunServerHostnameFieldName[] = "stunServerHostname";

  DeviceCommandRunRoutineJob();
  DeviceCommandRunRoutineJob(const DeviceCommandRunRoutineJob&) = delete;
  DeviceCommandRunRoutineJob& operator=(const DeviceCommandRunRoutineJob&) =
      delete;
  ~DeviceCommandRunRoutineJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  // RemoteCommandJob:
  bool ParseCommandPayload(const std::string& command_payload) override;
  void RunImpl(CallbackWithResult result_callback) override;

  void OnCrosHealthdResponseReceived(
      CallbackWithResult result_callback,
      ash::cros_healthd::mojom::RunRoutineResponsePtr response);

  // Which routine the DeviceCommandRunRoutineJob will run.
  ash::cros_healthd::mojom::DiagnosticRoutineEnum routine_enum_;
  // Parameters for the routine to be run. See
  // chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom for
  // details on the parameters accepted by each individual routine.
  base::Value::Dict params_dict_;

  base::WeakPtrFactory<DeviceCommandRunRoutineJob> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RUN_ROUTINE_JOB_H_
