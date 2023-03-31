// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_result_waiter.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Tests sending remote commands and verifying the results.
class RemoteCommandsTest : public DevicePolicyCrosBrowserTest {
 public:
  static constexpr int kFakePolicyPublicKeyVersion = 1;

  RemoteCommandsTest() = default;

  RemoteCommandsTest(const RemoteCommandsTest&) = delete;
  RemoteCommandsTest& operator=(const RemoteCommandsTest&) = delete;

  ~RemoteCommandsTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    DevicePolicyCrosBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    device_policy()->policy_data().set_public_key_version(
        kFakePolicyPublicKeyVersion);
    device_policy()->Build();
    session_manager_client()->set_device_policy(device_policy()->GetBlob());
  }

  void SendDeviceRemoteCommandsRequest() {
    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

    DeviceCloudPolicyManagerAsh* policy_manager =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceCloudPolicyManager();

    ASSERT_TRUE(policy_manager);

    ASSERT_TRUE(policy_manager->core()->client());

    RemoteCommandsService* const remote_commands_service =
        policy_manager->core()->remote_commands_service();
    remote_commands_service->FetchRemoteCommands();
  }

  em::RemoteCommandResult WaitForResult(int command_id) {
    em::RemoteCommandResult result =
        RemoteCommandsResultWaiter(
            policy_test_server_mixin_.server()->remote_commands_state(),
            command_id)
            .WaitAndGetResult();
    return result;
  }

  void AddPendingRemoteCommand(const em::RemoteCommand& command) {
    policy_test_server_mixin_.server()
        ->remote_commands_state()
        ->AddPendingRemoteCommand(command);
  }

  void StartTestServer() {
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(RemoteCommandsTest, DeviceSetVolume) {
  StartTestServer();

  em::RemoteCommand command;
  int command_id = 1;
  command.set_type(em::RemoteCommand_Type_DEVICE_SET_VOLUME);
  command.set_command_id(command_id);
  command.set_payload(R"({"volume": 50})");
  AddPendingRemoteCommand(command);

  SendDeviceRemoteCommandsRequest();
  em::RemoteCommandResult result = WaitForResult(command_id);

  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
}

}  // namespace policy
