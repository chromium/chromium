// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_set_volume_job.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

namespace em = enterprise_management;
using policy::CloudPolicyClient;
using policy::TestingRemoteCommandsServer;

const char kDMToken[] = "dmtoken";
const char kDeviceId[] = "kiosk-device";

// Test `CloudPolicyClient` that interacts with `TestingRemoteCommandsServer`.
class TestRemoteCommandsClient : public CloudPolicyClient {
 public:
  explicit TestRemoteCommandsClient(TestingRemoteCommandsServer* server)
      : CloudPolicyClient(nullptr /* service */,
                          nullptr /* url_loader_factory */,
                          CloudPolicyClient::DeviceDMTokenCallback()),
        server_(server) {
    dm_token_ = kDMToken;
  }
  TestRemoteCommandsClient(const TestRemoteCommandsClient&) = delete;
  TestRemoteCommandsClient& operator=(const TestRemoteCommandsClient&) = delete;

  ~TestRemoteCommandsClient() override = default;

 private:
  void FetchRemoteCommands(
      std::unique_ptr<policy::RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<em::RemoteCommandResult>& command_results,
      em::PolicyFetchRequest::SignatureType signature_type,
      RemoteCommandCallback callback) override {
    std::vector<em::SignedData> commands =
        server_->FetchCommands(std::move(last_command_id), command_results);

    // Asynchronously send the response from the DMServer back to client.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  policy::DM_STATUS_SUCCESS, commands));
  }

  void FetchPolicy() override {
    // Empty to avoid a crashing cloud policy fetch attempt in ash
  }

  raw_ptr<TestingRemoteCommandsServer> server_;
};

// workflow: COM_KIOSK_CUJ8_TASK1_WF1
constexpr char kKioskRemoteVolumeCommandTag[] =
    "screenplay-6ba34335-2f1f-4f78-a115-9149348a59fe";

void AddScreenplayTag(const std::string& screenplay_tag) {
  base::AddTagToTestResult("feature_id", screenplay_tag);
}

}  // namespace

// Kiosk tests with a fake device owner setup and a fake remote command client.
class KioskRemoteCommandTest : public KioskBaseTest {
 public:
  KioskRemoteCommandTest() {
    settings_helper_.Set(kDeviceOwner,
                         base::Value(test_owner_account_id_.GetUserEmail()));
    login_manager_.AppendRegularUsers(1);
  }

  void SetUpOnMainThread() override {
    KioskBaseTest::SetUpOnMainThread();

    remote_command_server_ = std::make_unique<TestingRemoteCommandsServer>();

    // Create RemoteCommandsService
    policy::BrowserPolicyConnectorAsh* const connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();

    policy_manager_ = connector->GetDeviceCloudPolicyManager();
    policy_manager_->StartConnection(std::make_unique<TestRemoteCommandsClient>(
                                         remote_command_server_.get()),
                                     connector->GetInstallAttributes());
    SetPublicKeyAndDeviceId();
  }

 protected:
  void SetPublicKeyAndDeviceId() {
    policy_manager_->core()
        ->store()
        ->set_policy_signature_public_key_for_testing(
            policy::PolicyBuilder::GetPublicTestKeyAsString());

    auto policy_data = std::make_unique<em::PolicyData>();
    policy_data->set_device_id(kDeviceId);
    policy_manager_->core()->store()->set_policy_data_for_testing(
        std::move(policy_data));
  }

  em::SignedData CreateSetVolumeRemoteCommand(int volume_level) {
    std::string command_payload;
    {
      base::Value::Dict root_dict;
      root_dict.Set(policy::DeviceCommandSetVolumeJob::kVolumeFieldName,
                    volume_level);
      base::JSONWriter::Write(root_dict, &command_payload);
    }
    em::SignedData signed_command =
        policy::SignedDataBuilder()
            .WithCommandId(remote_command_server_->GetNextCommandId())
            .WithTargetDeviceId(kDeviceId)
            .WithCommandType(em::RemoteCommand_Type_DEVICE_SET_VOLUME)
            .WithCommandPayload(command_payload)
            .Build();

    return signed_command;
  }

  em::RemoteCommandResult IssueCommandAndGetResponse(em::SignedData command) {
    using ServerResponseFuture =
        base::test::TestFuture<const em::RemoteCommandResult&>;
    ServerResponseFuture response_future;
    remote_command_server_->IssueCommand(command,
                                         response_future.GetCallback());

    policy::RemoteCommandsService* const remote_commands_service =
        policy_manager_->core()->remote_commands_service();
    remote_commands_service->FetchRemoteCommands();

    // Waits for (async) remote commands to finish
    return response_future.Get();
  }

  LoginManagerMixin login_manager_{
      &mixin_host_,
      {{LoginManagerMixin::TestUserInfo{test_owner_account_id_}}}};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};

  std::unique_ptr<TestingRemoteCommandsServer> remote_command_server_;
  policy::DeviceCloudPolicyManagerAsh* policy_manager_;
};

IN_PROC_BROWSER_TEST_F(KioskRemoteCommandTest, SetVolumeWithRemoteCommand) {
  AddScreenplayTag(kKioskRemoteVolumeCommandTag);

  constexpr int kInitVolumePercent = 50;
  constexpr int kExpectedVolumePercent = 72;

  // Set audio handler and initial volume
  ash::CrasAudioHandler* audio_handler = ash::CrasAudioHandler::Get();
  audio_handler->SetOutputVolumePercent(kInitVolumePercent);
  EXPECT_EQ(kInitVolumePercent, audio_handler->GetOutputVolumePercent());

  // Launch kiosk app
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  // Create a remote command, enqueue from the server, fetch from the client
  em::SignedData volume_command =
      CreateSetVolumeRemoteCommand(kExpectedVolumePercent);

  auto response = IssueCommandAndGetResponse(volume_command);

  // Check that remote command passed and the new volume level was set
  EXPECT_EQ(response.result(),
            em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), kExpectedVolumePercent);
}
}  // namespace ash
