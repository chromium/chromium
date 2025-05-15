// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string_view>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/scoped_observation.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/policy/remote_commands/device_commands_factory_ash.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/core/common/remote_commands/test_support/testing_remote_commands_server.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/power_manager/dbus-constants.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

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

policy::RemoteCommandBuilder NewRemoteCommandBuilder() {
  return policy::RemoteCommandBuilder().SetTargetDeviceId(kDeviceId);
}

// Observes requests sent to `PowerManagerClient` to restart the device.
class RestartRequestObserver : public chromeos::PowerManagerClient::Observer {
 public:
  explicit RestartRequestObserver(chromeos::PowerManagerClient* client) {
    CHECK_NE(client, nullptr);
    observation_.Observe(client);
  }
  RestartRequestObserver(const RestartRequestObserver&) = delete;
  RestartRequestObserver& operator=(const RestartRequestObserver&) = delete;
  ~RestartRequestObserver() override = default;

  // Waits until a restart request happens and returns its reason.
  power_manager::RequestRestartReason WaitForRestartRequest() {
    return reason_future_.Get();
  }

  // `chromeos::PowerManagerClient::Observer` implementation:
  void RestartRequested(power_manager::RequestRestartReason reason) override {
    reason_future_.SetValue(reason);
  }

 private:
  base::test::TestFuture<power_manager::RequestRestartReason> reason_future_;
  base::ScopedObservation<chromeos::PowerManagerClient, RestartRequestObserver>
      observation_{this};
};

class VolumeChangeObserver : public CrasAudioHandler::AudioObserver {
 public:
  explicit VolumeChangeObserver(CrasAudioHandler& handler) {
    observation_.Observe(&handler);
  }
  VolumeChangeObserver(const VolumeChangeObserver&) = delete;
  VolumeChangeObserver& operator=(const VolumeChangeObserver&) = delete;
  ~VolumeChangeObserver() override = default;

  // `CrasAudioHandler::AudioObserver` implementation:
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override {
    waiter_.SetValue(volume);
  }

  int WaitForVolumeChange() {
    EXPECT_TRUE(waiter_.Wait()) << "Never received a volume changed event";
    return waiter_.Take();
  }

 private:
  base::test::TestFuture<int> waiter_;
  base::ScopedObservation<CrasAudioHandler, CrasAudioHandler::AudioObserver>
      observation_{this};
};

}  // namespace

class KioskRemoteCommandTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskRemoteCommandTest() {
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine("AllowChromeAppsInKioskSessions",
                                             "");
  }
  KioskRemoteCommandTest(const KioskRemoteCommandTest&) = delete;
  KioskRemoteCommandTest& operator=(const KioskRemoteCommandTest&) = delete;

  ~KioskRemoteCommandTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    // On real hardware volume change events are reported asynchronous, so
    // ensure the test behaves similar so they are realistic (and catch the
    // timing issues this can cause).
    FakeCrasAudioClient::Get()->send_volume_change_events_asynchronous();
  }

  em::RemoteCommandResult IssueCommandAndGetResponse(
      em::RemoteCommand command) {
    return remote_commands_service_mixin_.SendRemoteCommand(command);
  }

  const KioskMixin::Config& kiosk_mixin_config() { return GetParam(); }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/kiosk_mixin_config()};

 private:
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  policy::RemoteCommandsServiceMixin remote_commands_service_mixin_{
      mixin_host_, policy_test_server_mixin_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(KioskRemoteCommandTest, SetVolumeWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteVolumeCommandTag);

  constexpr int kInitVolumePercent = 50;
  constexpr int kExpectedVolumePercent = 72;

  // Set audio handler and initial volume.
  CrasAudioHandler& audio_handler = CHECK_DEREF(CrasAudioHandler::Get());
  VolumeChangeObserver audio_observer(audio_handler);
  audio_handler.SetOutputVolumePercent(kInitVolumePercent);
  audio_observer.WaitForVolumeChange();
  ASSERT_EQ(kInitVolumePercent, audio_handler.GetOutputVolumePercent());

  ASSERT_TRUE(WaitKioskLaunched());

  // Create a remote command, enqueue from the server and fetch from the client.
  auto response = IssueCommandAndGetResponse(
      NewRemoteCommandBuilder()
          .SetType(em::RemoteCommand_Type_DEVICE_SET_VOLUME)
          .SetPayload(
              base::WriteJson(base::Value::Dict()  //
                                  .Set("volume", kExpectedVolumePercent))
                  .value())
          .Build());

  // Check that remote command succeeded and the new volume level was set.
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
  audio_observer.WaitForVolumeChange();
  EXPECT_EQ(kExpectedVolumePercent, audio_handler.GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_P(KioskRemoteCommandTest, RebootWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteRebootCommandTag);

  ASSERT_TRUE(WaitKioskLaunched());

  // Start observing restart requests in `PowerManagerClient`.
  RestartRequestObserver observer(chromeos::PowerManagerClient::Get());

  // Create a remote command, enqueue from the server, fetch from the client.
  auto response = IssueCommandAndGetResponse(
      NewRemoteCommandBuilder()
          .SetType(em::RemoteCommand_Type_DEVICE_REBOOT)
          .Build());

  // Check that remote cmd passed and reboot was requested (via observer event).
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
  EXPECT_EQ(power_manager::REQUEST_RESTART_REMOTE_ACTION_REBOOT,
            observer.WaitForRestartRequest());
}

IN_PROC_BROWSER_TEST_P(KioskRemoteCommandTest, ScreenshotWithRemoteCommand) {
  base::AddFeatureIdTagToTestResult(kKioskRemoteScreenshotCommandTag);

  ASSERT_TRUE(WaitKioskLaunched());

  // Skips real image upload.
  // TODO(crbug.com/269432279): Try real upload with embedded test server.
  policy::DeviceCommandsFactoryAsh::set_commands_for_testing(true);

  auto response = IssueCommandAndGetResponse(
      NewRemoteCommandBuilder()
          .SetType(em::RemoteCommand_Type_DEVICE_SCREENSHOT)
          .SetPayload(R"( {"fileUploadUrl": "http://example.com/upload"} )")
          .Build());

  // Check that the remote command succeeded.
  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS,
            response.result());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskRemoteCommandTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
