// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/scoped_observation.h"
#include "base/test/gtest_tags.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_set_volume_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_commands_factory_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace ash {

namespace {

namespace em = enterprise_management;
using policy::CloudPolicyClient;
using policy::TestingRemoteCommandsServer;

const char kDMToken[] = "dmtoken";
const char kDeviceId[] = "kiosk-device";

// workflow: COM_KIOSK_CUJ8_TASK1_WF1
constexpr char kKioskRemoteVolumeCommandTag[] =
    "screenplay-6ba34335-2f1f-4f78-a115-9149348a59fe";

// workflow: COM_KIOSK_CUJ8_TASK2_WF1
constexpr char kKioskRemoteRebootCommandTag[] =
    "screenplay-95efa645-3d98-4638-9e5c-c0fe6f2c150d";

// workflow: COM_KIOSK_CUJ8_TASK3_WF1
constexpr char kKioskRemoteScreenshotCommandTag[] =
    "screenplay-110c74bf-7c94-4e88-8904-95b6bbc7d649";

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

class TestRebootObserver : public chromeos::PowerManagerClient::Observer {
 public:
  TestRebootObserver() = default;
  ~TestRebootObserver() override = default;
  TestRebootObserver(const TestRebootObserver&) = delete;
  TestRebootObserver& operator=(const TestRebootObserver&) = delete;

  power_manager::RequestRestartReason Get() { return reboot_future_.Get(); }

  // chromeos::PowerManagerClient::Observer
  void RestartRequested(power_manager::RequestRestartReason reason) override {
    reboot_future_.SetValue(reason);
  }

 private:
  base::test::TestFuture<power_manager::RequestRestartReason> reboot_future_;
};

class TestAudioObserver : public ash::CrasAudioHandler::AudioObserver {
 public:
  explicit TestAudioObserver(ash::CrasAudioHandler& handler)
      : observation_(this) {
    observation_.Observe(&handler);
  }
  TestAudioObserver(const TestAudioObserver&) = delete;
  TestAudioObserver& operator=(const TestAudioObserver&) = delete;
  ~TestAudioObserver() override = default;

  // `ash::CrasAudioHandler::AudioObserver` implementation:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override {
    waiter_.AddValue(volume);
  }

  void WaitForVolumeChange() {
    EXPECT_TRUE(waiter_.Wait()) << "Never received a volume changed event";
    waiter_.Take();
  }

 private:
  base::test::RepeatingTestFuture<int> waiter_;
  base::ScopedObservation<ash::CrasAudioHandler,
                          ash::CrasAudioHandler::AudioObserver>
      observation_;
};

}  // namespace

// Kiosk tests with a fake device owner setup and a fake remote command client.
class KioskRemoteCommandTest : public KioskBaseTest {
 public:
  KioskRemoteCommandTest() {
    // Skip initial policy setup is needed to do a custom StartConnection below
    device_state_.set_skip_initial_policy_setup(true);

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

    // On real hardware volume change events are reported asynchronous, so
    // ensure the test behaves similar so they are realistic (and catch the
    // timing issues this can cause).
    ash::FakeCrasAudioClient::Get()->send_volume_change_events_asynchronous();
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

  em::SignedData CreateRebootRemoteCommand() {
    em::SignedData signed_command =
        policy::SignedDataBuilder()
            .WithCommandId(remote_command_server_->GetNextCommandId())
            .WithTargetDeviceId(kDeviceId)
            .WithCommandType(em::RemoteCommand_Type_DEVICE_REBOOT)
            .Build();
    return signed_command;
  }

  em::SignedData CreateScreenshotRemoteCommand() {
    constexpr char kMockUploadUrl[] = "http://example.com/upload";
    std::string command_payload;
    {
      base::Value::Dict root_dict;
      root_dict.Set(policy::DeviceCommandScreenshotJob::kUploadUrlFieldName,
                    kMockUploadUrl);
      base::JSONWriter::Write(root_dict, &command_payload);
    }
    em::SignedData signed_command =
        policy::SignedDataBuilder()
            .WithCommandId(remote_command_server_->GetNextCommandId())
            .WithTargetDeviceId(kDeviceId)
            .WithCommandType(em::RemoteCommand_Type_DEVICE_SCREENSHOT)
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
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  std::unique_ptr<TestingRemoteCommandsServer> remote_command_server_;
  policy::DeviceCloudPolicyManagerAsh* policy_manager_;
};

IN_PROC_BROWSER_TEST_F(KioskRemoteCommandTest, SetVolumeWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteVolumeCommandTag);

  constexpr int kInitVolumePercent = 50;
  constexpr int kExpectedVolumePercent = 72;

  // Set audio handler and initial volume
  ash::CrasAudioHandler* audio_handler = ash::CrasAudioHandler::Get();
  TestAudioObserver audio_observer(*audio_handler);
  audio_handler->SetOutputVolumePercent(kInitVolumePercent);
  audio_observer.WaitForVolumeChange();
  ASSERT_EQ(kInitVolumePercent, audio_handler->GetOutputVolumePercent());

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
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
  audio_observer.WaitForVolumeChange();
  EXPECT_EQ(kExpectedVolumePercent, audio_handler->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(KioskRemoteCommandTest, RebootWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteRebootCommandTag);

  // Launch kiosk app
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  // Get PowerManagerClient and start observing a restart request
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  ASSERT_NE(power_manager_client, nullptr);

  TestRebootObserver observer;
  power_manager_client->AddObserver(&observer);

  // Create a remote command, enqueue from the server, fetch from the client
  em::SignedData reboot_command = CreateRebootRemoteCommand();
  auto response = IssueCommandAndGetResponse(reboot_command);

  // Check that remote cmd passed and reboot was requested (via observer event)
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
  EXPECT_EQ(power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT,
            observer.Get());
}

IN_PROC_BROWSER_TEST_F(KioskRemoteCommandTest, ScreenshotWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteScreenshotCommandTag);

  // Launch kiosk app
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/false,
                              /*keep_app_open=*/true);

  // Create a remote command, enqueue from the server, fetch from the client
  em::SignedData screenshot_command = CreateScreenshotRemoteCommand();

  // skips real image upload
  // TODO(b/269432279): Try real upload with local url and EmbeddedTestServer
  policy::DeviceCommandsFactoryAsh::set_commands_for_testing(true);

  auto response = IssueCommandAndGetResponse(screenshot_command);

  // Check that remote cmd passed
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
}
}  // namespace ash
