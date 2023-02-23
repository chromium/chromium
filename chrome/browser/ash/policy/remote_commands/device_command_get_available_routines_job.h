// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_GET_AVAILABLE_ROUTINES_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_GET_AVAILABLE_ROUTINES_JOB_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_diagnostics.mojom.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

// This class implements a RemoteCommandJob that retrieves a list of diagnostic
// routines supported by the platform. The RemoteCommandsQueue owns all
// instances of this class.
class DeviceCommandGetAvailableRoutinesJob : public RemoteCommandJob {
 public:
  DeviceCommandGetAvailableRoutinesJob();
  DeviceCommandGetAvailableRoutinesJob(
      const DeviceCommandGetAvailableRoutinesJob&) = delete;
  DeviceCommandGetAvailableRoutinesJob& operator=(
      const DeviceCommandGetAvailableRoutinesJob&) = delete;
  ~DeviceCommandGetAvailableRoutinesJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;

  void OnCrosHealthdResponseReceived(
      CallbackWithResult result_callback,
      const std::vector<ash::cros_healthd::mojom::DiagnosticRoutineEnum>&
          available_routines);

  base::WeakPtrFactory<DeviceCommandGetAvailableRoutinesJob> weak_ptr_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_GET_AVAILABLE_ROUTINES_JOB_H_
