// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <string>
#include <utility>

#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

DeviceCommandFetchSupportPacketJob::DeviceCommandFetchSupportPacketJob() =
    default;

DeviceCommandFetchSupportPacketJob::~DeviceCommandFetchSupportPacketJob() =
    default;

enterprise_management::RemoteCommand_Type
DeviceCommandFetchSupportPacketJob::GetType() const {
  return enterprise_management::RemoteCommand_Type_FETCH_SUPPORT_PACKET;
}

void DeviceCommandFetchSupportPacketJob::RunImpl(
    CallbackWithResult result_callback) {
  result_callback_ = std::move(result_callback);
  // TODO(b/264399756): The contents will be filled in the follow-up CL.
}

bool DeviceCommandFetchSupportPacketJob::ParseCommandPayload(
    const std::string& command_payload) {
  return support_packet_details_.ParseFromString(command_payload);
}

}  // namespace policy
