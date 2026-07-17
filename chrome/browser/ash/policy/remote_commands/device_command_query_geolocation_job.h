// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_QUERY_GEOLOCATION_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_QUERY_GEOLOCATION_JOB_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

class CloudPolicyStore;

class DeviceCommandQueryGeolocationJob : public RemoteCommandJob {
 public:
  // `local_state` must be non-null and must outlive `this`.
  // `device_cloud_policy_store` may be null. If non-null, it must outlive
  // `this`.
  DeviceCommandQueryGeolocationJob(
      PrefService* local_state,
      const CloudPolicyStore* device_cloud_policy_store);
  ~DeviceCommandQueryGeolocationJob() override;

  DeviceCommandQueryGeolocationJob(const DeviceCommandQueryGeolocationJob&) =
      delete;
  DeviceCommandQueryGeolocationJob& operator=(
      const DeviceCommandQueryGeolocationJob&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // `local_state` must be non-null and must outlive the created notification.
  static void ShowLocationReportedNotificationIfNeeded(
      PrefService* local_state);

  // RemoteCommandJob:
  enterprise_management::RemoteCommand::Type GetType() const override;

 private:
  // RemoteCommandJob:
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(CallbackWithResult result_callback) override;

  void RunImplInternal(CallbackWithResult result_callback, bool retried);

  std::optional<enterprise_management::QueryGeolocationCommandResultCode>
  CheckIfCommandIsAllowed() const;

  void OnLocationResponse(CallbackWithResult result_callback,
                          const ash::Geoposition& position,
                          bool server_error,
                          const base::TimeDelta elapsed);

  const raw_ref<PrefService> local_state_;
  const raw_ptr<const CloudPolicyStore> policy_store_;
  base::WeakPtrFactory<DeviceCommandQueryGeolocationJob> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_QUERY_GEOLOCATION_JOB_H_
