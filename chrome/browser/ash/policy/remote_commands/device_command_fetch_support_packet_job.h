// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

class DeviceCommandFetchSupportPacketJob : public RemoteCommandJob {
 public:
  DeviceCommandFetchSupportPacketJob();

  DeviceCommandFetchSupportPacketJob(
      const DeviceCommandFetchSupportPacketJob&) = delete;
  DeviceCommandFetchSupportPacketJob& operator=(
      const DeviceCommandFetchSupportPacketJob&) = delete;

  ~DeviceCommandFetchSupportPacketJob() override;

  // RemoteCommandJob:
  enterprise_management::RemoteCommand_Type GetType() const override;

 protected:
  // RemoteCommandJob:
  void RunImpl(CallbackWithResult result_callback) override;
  // Parses `command_payload` into SupportPacketDetails proto message.
  bool ParseCommandPayload(const std::string& command_payload) override;

 private:
  // The details of requested support packet. Contains details like data
  // collectors, PII types, case ID etc.
  support_tool::SupportPacketDetails support_packet_details_;
  // The callback to run when the execution of RemoteCommandJob has finished.
  CallbackWithResult result_callback_;
  base::WeakPtrFactory<DeviceCommandFetchSupportPacketJob> weak_ptr_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_H_
