// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_SERVICE_MIXIN_H_
#define CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_SERVICE_MIXIN_H_

#include <cstdint>

#include "base/allocator/partition_allocator/pointers/raw_ref.h"
#include "base/check_deref.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace ash {
class EmbeddedPolicyTestServerMixin;
}  // namespace ash

namespace policy {

class RemoteCommandsService;
class RemoteCommandsState;

// A test utility that makes it easy to test sending and executing remote
// commands.
class RemoteCommandsServiceMixin : public InProcessBrowserTestMixin {
 public:
  RemoteCommandsServiceMixin(InProcessBrowserTestMixinHost& host,
                             ash::EmbeddedPolicyTestServerMixin& test_server);

  RemoteCommandsServiceMixin(const RemoteCommandsServiceMixin&) = delete;
  RemoteCommandsServiceMixin& operator=(const RemoteCommandsServiceMixin&) =
      delete;
  ~RemoteCommandsServiceMixin() override;

  // Sends the given remote command to the `RemoteCommandsService` and returns
  // the execution result.
  enterprise_management::RemoteCommandResult SendRemoteCommand(
      const enterprise_management::RemoteCommand& command);

  // Adds a pending remote command. These will be sent to the
  // `RemoteCommandsService` the next time it fetches remote commands, which
  // your test can trigger by calling `SendDeviceRemoteCommandsRequest()`.
  void AddPendingRemoteCommand(
      const enterprise_management::RemoteCommand& command);

  // Sends all pending remote commands to the `RemoteCommandsService`, causing
  // it to execute these remote commands asynchronously.
  void SendDeviceRemoteCommandsRequest();

  // Waits until the remote command with the given command_id` has been
  // executed.
  // Note that your test is responsible to ensure the `RemoteCommandsService`
  // fetches and executes this remote command, which you can do by calling
  // `SendDeviceRemoteCommandsRequest()` prior to calling this.
  enterprise_management::RemoteCommandResult WaitForResult(int64_t command_id);

 private:
  RemoteCommandsService& remote_commands_service();
  RemoteCommandsState& remote_commands_state();

  const raw_ref<ash::EmbeddedPolicyTestServerMixin> policy_test_server_mixin_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_SERVICE_MIXIN_H_
