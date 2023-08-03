// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RESET_EUICC_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RESET_EUICC_JOB_H_

#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "dbus/object_path.h"

namespace policy {

// This class implements a RemoteCommandJob that clears the cellular EUICC
// (Embedded Universal Integrated Circuit Card) on the device, that stores all
// installed eSIM profiles. This effectively removes all eSIM profiles installed
// on the device.
class DeviceCommandResetEuiccJob : public RemoteCommandJob {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ResetEuiccResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesResetFailed = 2,
    kMaxValue = kHermesResetFailed
  };

  static const char kResetEuiccNotificationId[];

  DeviceCommandResetEuiccJob();
  DeviceCommandResetEuiccJob(const DeviceCommandResetEuiccJob&) = delete;
  DeviceCommandResetEuiccJob& operator=(const DeviceCommandResetEuiccJob&) =
      delete;
  ~DeviceCommandResetEuiccJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  static void RecordResetEuiccResult(ResetEuiccResult result);

  // RemoteCommandJob:
  bool IsExpired(base::TimeTicks now) override;
  void RunImpl(CallbackWithResult result_callback) override;

  void OnResetMemoryResponse(CallbackWithResult result_callback,
                             base::Time reset_euicc_start_time,
                             bool success);
  void RunResultCallback(CallbackWithResult callback, ResultType result);
  void ShowResetEuiccNotification();

  base::WeakPtrFactory<DeviceCommandResetEuiccJob> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RESET_EUICC_JOB_H_
