// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_writer.h"
#include "base/scoped_observation.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/remote_commands/device_commands_factory_ash.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/dbus/power/power_manager_client.h"
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
    waiter_.SetValue(volume);
  }

  int WaitForVolumeChange() {
    EXPECT_TRUE(waiter_.Wait()) << "Never received a volume changed event";
    return waiter_.Take();
  }

 private:
  base::test::TestFuture<int> waiter_;
  base::ScopedObservation<ash::CrasAudioHandler,
                          ash::CrasAudioHandler::AudioObserver>
      observation_;
};

}  // namespace

// Kiosk tests with a fake device owner setup and a remote commands server
// configured.
class KioskRemoteCommandTest : public KioskBaseTest {
 public:
  KioskRemoteCommandTest() {
    settings_helper_.SetString(kDeviceOwner,
                               test_owner_account_id_.GetUserEmail());
    login_manager_.AppendRegularUsers(1);
  }

  void SetUpOnMainThread() override {
    KioskBaseTest::SetUpOnMainThread();

    // On real hardware volume change events are reported asynchronous, so
    // ensure the test behaves similar so they are realistic (and catch the
    // timing issues this can cause).
    ash::FakeCrasAudioClient::Get()->send_volume_change_events_asynchronous();
  }

  policy::RemoteCommandBuilder BuildRemoteCommand() {
    return policy::RemoteCommandBuilder().SetTargetDeviceId(kDeviceId);
  }

  em::RemoteCommandResult IssueCommandAndGetResponse(
      em::RemoteCommand command) {
    return remote_commands_service_mixin_.SendRemoteCommand(command);
  }

  void LaunchKioskApp() {
    StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
    WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                                /*terminate_app=*/false,
                                /*keep_app_open=*/true);
  }

 private:
  LoginManagerMixin login_manager_{
      &mixin_host_,
      {{LoginManagerMixin::TestUserInfo{test_owner_account_id_}}}};

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  policy::RemoteCommandsServiceMixin remote_commands_service_mixin_{
      mixin_host_, policy_test_server_mixin_};
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

  LaunchKioskApp();

  // Create a remote command, enqueue from the server, fetch from the client
  auto response = IssueCommandAndGetResponse(
      BuildRemoteCommand()
          .SetType(em::RemoteCommand_Type_DEVICE_SET_VOLUME)
          .SetPayload(
              base::WriteJson(base::Value::Dict()  //
                                  .Set("volume", kExpectedVolumePercent))
                  .value())
          .Build());

  // Check that remote command passed and the new volume level was set
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
  audio_observer.WaitForVolumeChange();
  EXPECT_EQ(kExpectedVolumePercent, audio_handler->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(KioskRemoteCommandTest, RebootWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteRebootCommandTag);

  LaunchKioskApp();

  // Get PowerManagerClient and start observing a restart request
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  ASSERT_NE(power_manager_client, nullptr);

  TestRebootObserver observer;
  power_manager_client->AddObserver(&observer);

  // Create a remote command, enqueue from the server, fetch from the client
  auto response = IssueCommandAndGetResponse(
      BuildRemoteCommand()
          .SetType(em::RemoteCommand_Type_DEVICE_REBOOT)
          .Build());

  // Check that remote cmd passed and reboot was requested (via observer event)
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
  EXPECT_EQ(power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT,
            observer.Get());
}

IN_PROC_BROWSER_TEST_F(KioskRemoteCommandTest, ScreenshotWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteScreenshotCommandTag);

  LaunchKioskApp();

  // skips real image upload
  // TODO(b/269432279): Try real upload with local url and EmbeddedTestServer
  policy::DeviceCommandsFactoryAsh::set_commands_for_testing(true);

  auto response = IssueCommandAndGetResponse(
      BuildRemoteCommand()
          .SetType(em::RemoteCommand_Type_DEVICE_SCREENSHOT)
          .SetPayload(R"( {"fileUploadUrl": "http://example.com/upload"} )")
          .Build());

  // Check that remote cmd passed
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
}

}  // namespace ash
