// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_SERVICE_MIXIN_H_
#define CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_SERVICE_MIXIN_H_

#include <cstdint>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
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

  // Sets the initial command id with the given id, the next commands ids will
  // be incrementally increasing by 1.
  // Note fake_dmserver's implementation of "acknowledging" remote commands
  // relies on the id strictly increasing, if this is called with an id which is
  // lower than a previously issued remote command, fake_dmserver will miss some
  // ack messages.
  void SetCurrentIdForTesting(int64_t id);

  // Sends the given remote command to the `RemoteCommandsService` and returns
  // the execution result.
  enterprise_management::RemoteCommandResult SendRemoteCommand(
      const enterprise_management::RemoteCommand& command);

  // Adds a pending remote command. These will be sent to the
  // `RemoteCommandsService` the next time it fetches remote commands, which
  // your test can trigger by calling `SendDeviceRemoteCommandsRequest()`.
  // This function assigns a `command_id` and discards any `command_id` that is
  // present in the passed `command`. If a specific `command_id` is required
  // then use `SetCurrentIdForTesting()` to set the initial id. Returns the
  // assigned `command_id`.
  int64_t AddPendingRemoteCommand(
      const enterprise_management::RemoteCommand& command);

  // Sends all pending remote commands to the `RemoteCommandsService`, causing
  // it to execute these remote commands asynchronously.
  void SendDeviceRemoteCommandsRequest();

  // Waits until the remote command with the given `command_id` has been
  // executed.
  // Note that your test is responsible to ensure the `RemoteCommandsService`
  // fetches and executes this remote command, which you can do by calling
  // `SendDeviceRemoteCommandsRequest()` prior to calling this.
  enterprise_management::RemoteCommandResult WaitForResult(int64_t command_id);

  // Waits until the remote command with the given `command_id` has been
  // acknowledged.
  // Note that your test is responsible to ensure the `RemoteCommandsService`
  // fetches and executes this remote command, which you can do by calling
  // `SendDeviceRemoteCommandsRequest()` prior to calling this.
  void WaitForAcked(int64_t command_id);

 private:
  RemoteCommandsService& remote_commands_service();
  RemoteCommandsState& remote_commands_state();

  const raw_ref<ash::EmbeddedPolicyTestServerMixin> policy_test_server_mixin_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_REMOTE_COMMANDS_SERVICE_MIXIN_H_
