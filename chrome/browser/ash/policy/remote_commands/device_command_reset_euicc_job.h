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
  DeviceCommandResetEuiccJob();
  DeviceCommandResetEuiccJob(const DeviceCommandResetEuiccJob&) = delete;
  DeviceCommandResetEuiccJob& operator=(const DeviceCommandResetEuiccJob&) =
      delete;
  ~DeviceCommandResetEuiccJob() override;

  static const char kResetEuiccNotificationId[];

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 private:
  friend class DeviceCommandResetEuiccJobTest;
  FRIEND_TEST_ALL_PREFIXES(DeviceCommandResetEuiccJobTest, ResetEuicc);
  FRIEND_TEST_ALL_PREFIXES(DeviceCommandResetEuiccJobTest, ResetEuiccFailure);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ResetEuiccResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesResetFailed = 2,
    kMaxValue = kHermesResetFailed
  };
  static void RecordResetEuiccResult(ResetEuiccResult result);

  // RemoteCommandJob:
  void RunImpl(CallbackWithResult succeeded_callback,
               CallbackWithResult failed_callback) override;

  CallbackWithResult CreateTimedResetMemorySuccessCallback(
      CallbackWithResult success_callback);

  void OnResetMemoryResponse(CallbackWithResult succeeded_callback,
                             CallbackWithResult failed_callback,
                             bool status);
  void RunResultCallback(CallbackWithResult callback);
  void ShowResetEuiccNotification();

  base::WeakPtrFactory<DeviceCommandResetEuiccJob> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_RESET_EUICC_JOB_H_
