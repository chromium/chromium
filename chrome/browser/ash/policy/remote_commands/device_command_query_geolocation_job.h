// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_QUERY_GEOLOCATION_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_QUERY_GEOLOCATION_JOB_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

class DeviceCloudPolicyManagerAsh;

class DeviceCommandQueryGeolocationJob : public RemoteCommandJob {
 public:
  explicit DeviceCommandQueryGeolocationJob(
      const DeviceCloudPolicyManagerAsh* policy_manager);
  ~DeviceCommandQueryGeolocationJob() override;

  DeviceCommandQueryGeolocationJob(const DeviceCommandQueryGeolocationJob&) =
      delete;
  DeviceCommandQueryGeolocationJob& operator=(
      const DeviceCommandQueryGeolocationJob&) = delete;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand::Type GetType() const override;

 private:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;

  std::optional<enterprise_management::QueryGeolocationCommandResultCode>
  CheckIfCommandIsAllowed() const;

  void OnLocationResponse(CallbackWithResult result_callback,
                          const ash::Geoposition& position,
                          bool server_error,
                          const base::TimeDelta elapsed);

  const raw_ptr<const DeviceCloudPolicyManagerAsh> policy_manager_;
  base::WeakPtrFactory<DeviceCommandQueryGeolocationJob> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_QUERY_GEOLOCATION_JOB_H_
