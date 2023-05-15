// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Tests sending remote commands and verifying the results.
class RemoteCommandsTest : public DevicePolicyCrosBrowserTest {
 public:
  RemoteCommandsTest() = default;

  RemoteCommandsTest(const RemoteCommandsTest&) = delete;
  RemoteCommandsTest& operator=(const RemoteCommandsTest&) = delete;

  ~RemoteCommandsTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    embedded_test_server()->StartAcceptingConnections();
  }

  em::RemoteCommandResult SendRemoteCommand(
      const enterprise_management::RemoteCommand& command) {
    return remote_commands_service_mixin_.SendRemoteCommand(command);
  }

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  RemoteCommandsServiceMixin remote_commands_service_mixin_{
      mixin_host_, policy_test_server_mixin_};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(RemoteCommandsTest, DeviceSetVolume) {
  em::RemoteCommandResult result =
      SendRemoteCommand(RemoteCommandBuilder()
                            .SetType(em::RemoteCommand_Type_DEVICE_SET_VOLUME)
                            .SetPayload(R"({"volume": 50})")
                            .Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
}

}  // namespace policy
