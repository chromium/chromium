// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"

#include "base/check_deref.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_result_waiter.h"
#include "components/policy/test_support/remote_commands_state.h"

namespace policy {

RemoteCommandsServiceMixin::RemoteCommandsServiceMixin(
    InProcessBrowserTestMixinHost& host,
    ash::EmbeddedPolicyTestServerMixin& test_server)
    : InProcessBrowserTestMixin(&host),
      policy_test_server_mixin_(test_server) {}

RemoteCommandsServiceMixin::~RemoteCommandsServiceMixin() = default;

enterprise_management::RemoteCommandResult
RemoteCommandsServiceMixin::SendRemoteCommand(
    const enterprise_management::RemoteCommand& command) {
  int64_t command_id = AddPendingRemoteCommand(command);
  SendDeviceRemoteCommandsRequest();
  return WaitForResult(command_id);
}

int64_t RemoteCommandsServiceMixin::AddPendingRemoteCommand(
    const enterprise_management::RemoteCommand& command) {
  return remote_commands_state().AddPendingRemoteCommand(command);
}

void RemoteCommandsServiceMixin::SendDeviceRemoteCommandsRequest() {
  remote_commands_service().FetchRemoteCommands();
}

enterprise_management::RemoteCommandResult
RemoteCommandsServiceMixin::WaitForResult(int64_t command_id) {
  return RemoteCommandsResultWaiter(&remote_commands_state(), command_id)
      .WaitAndGetResult();
}

void RemoteCommandsServiceMixin::WaitForAcked(int64_t command_id) {
  return RemoteCommandsResultWaiter(&remote_commands_state(), command_id)
      .WaitAndGetAck();
}

RemoteCommandsService& RemoteCommandsServiceMixin::remote_commands_service() {
  DeviceCloudPolicyManagerAsh& policy_manager =
      CHECK_DEREF(g_browser_process->platform_part()
                      ->browser_policy_connector_ash()
                      ->GetDeviceCloudPolicyManager());

  return CHECK_DEREF(policy_manager.core()->remote_commands_service());
}

RemoteCommandsState& RemoteCommandsServiceMixin::remote_commands_state() {
  return CHECK_DEREF(
      policy_test_server_mixin_->server()->remote_commands_state());
}

void RemoteCommandsServiceMixin::SetCurrentIdForTesting(int64_t id) {
  remote_commands_state().SetCurrentIdForTesting(id);
}

}  // namespace policy
