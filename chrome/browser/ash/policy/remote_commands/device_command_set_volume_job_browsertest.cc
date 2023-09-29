// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Use a number larger than int32 to catch truncation errors.
const int64_t kInitialCommandId = (1LL << 35) + 1;

// Tests sending remote commands and verifying the results.
class DeviceCommandSetVolumeBrowserTest : public DevicePolicyCrosBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    remote_commands_service_mixin_.SetCurrentIdForTesting(kInitialCommandId);
  }

  em::RemoteCommandResult SendRemoteCommand(
      const enterprise_management::RemoteCommand& command) {
    return remote_commands_service_mixin_.SendRemoteCommand(command);
  }

  void SetInitialCommandId(int64_t id) {
    remote_commands_service_mixin_.SetCurrentIdForTesting(id);
  }

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  RemoteCommandsServiceMixin remote_commands_service_mixin_{
      mixin_host_, policy_test_server_mixin_};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(DeviceCommandSetVolumeBrowserTest, DeviceSetVolume) {
  em::RemoteCommandResult result =
      SendRemoteCommand(RemoteCommandBuilder()
                            .SetType(em::RemoteCommand::DEVICE_SET_VOLUME)
                            .SetPayload(R"({"volume": 50})")
                            .Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
}

IN_PROC_BROWSER_TEST_F(DeviceCommandSetVolumeBrowserTest,
                       ShouldWorkWith64BitCommandIds) {
  SetInitialCommandId(112233445566778899LL);
  em::RemoteCommandResult result =
      SendRemoteCommand(RemoteCommandBuilder()
                            .SetType(em::RemoteCommand::DEVICE_SET_VOLUME)
                            .SetPayload(R"({"volume": 50})")
                            .SetCommandId(112233445566778899)
                            .Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
}

}  // namespace policy
