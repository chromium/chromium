// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_ash_feature_client.h"

#include <cstdlib>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

constexpr char kCrostiniVmId[] = "Linux";
constexpr char kPluginVmId[] = "PluginVm";
constexpr char kBorealisId[] = "Borealis";

class VideoConferenceAshfeatureClientTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kFeatureManagementVideoConference);

    InProcessBrowserTest::SetUp();
  }

  // Update the permission of current `app_id`.
  void UpdateAppPermision(const std::string& app_id,
                          bool has_camera_permission,
                          bool has_microphone_permission) {
    auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    if (app_id == kCrostiniVmId) {
      prefs->SetBoolean(crostini::prefs::kCrostiniMicAllowed,
                        has_microphone_permission);
      CHECK(!has_camera_permission)
          << "Camera is not supported for CrostiniVm yet.";
    }
    if (app_id == kBorealisId) {
      prefs->SetBoolean(borealis::prefs::kBorealisMicAllowed,
                        has_microphone_permission);
      CHECK(!has_camera_permission)
          << "Camera is not supported for CrostiniVm yet.";
    }
    if (app_id == kPluginVmId) {
      prefs->SetBoolean(plugin_vm::prefs::kPluginVmCameraAllowed,
                        has_camera_permission);
      prefs->SetBoolean(plugin_vm::prefs::kPluginVmMicAllowed,
                        has_microphone_permission);
    }
  }

  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> GetMediaApps() {
    std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> media_app_info;

    VideoConferenceAshFeatureClient::Get()->GetMediaApps(
        base::BindLambdaForTesting(
            [&media_app_info](
                std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>
                    result) { media_app_info = std::move(result); }));

    return media_app_info;
  }

  // Returns current VideoConferenceMediaState in the VideoConferenceManagerAsh
  VideoConferenceMediaState GetMediaStateInVideoConferenceManagerAsh() {
    return crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->video_conference_manager_ash()
        ->GetAggregatedState();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VideoConferenceAshfeatureClientTest, GetMediaApps) {
  {
    // Notifying Crostini is using Mic.
    VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
        VmCameraMicManager::VmType::kCrostiniVm,
        VmCameraMicManager::DeviceType::kMic, true);

    // GetMediaApps should return kCrostinId.
    auto media_app_info = GetMediaApps();
    ASSERT_EQ(media_app_info.size(), 1u);
    const auto& info0 = media_app_info[0];

    crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
        crosapi::mojom::VideoConferenceMediaAppInfo::New(
            /*id=*/info0->id,
            /*last_activity_time=*/info0->last_activity_time,
            /*is_capturing_camera=*/false,
            /*is_capturing_microphone=*/true,
            /*is_capturing_screen=*/false,
            /*title=*/base::UTF8ToUTF16(std::string(kCrostiniVmId)),
            /*url=*/std::nullopt,
            /*app_type=*/crosapi::mojom::VideoConferenceAppType::kCrostiniVm);

    EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));

    // Stop accessing should remove the app.
    VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
        VmCameraMicManager::VmType::kCrostiniVm,
        VmCameraMicManager::DeviceType::kMic, false);

    EXPECT_TRUE(GetMediaApps().empty());
  }

  {
    // Notifying PluginVm is using Mic and Camera.
    VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
        VmCameraMicManager::VmType::kPluginVm,
        VmCameraMicManager::DeviceType::kMic, true);
    VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
        VmCameraMicManager::VmType::kPluginVm,
        VmCameraMicManager::DeviceType::kCamera, true);

    // GetMediaApps should return PluginVm.
    auto media_app_info = GetMediaApps();
    ASSERT_EQ(media_app_info.size(), 1u);
    const auto& info0 = media_app_info[0];

    crosapi::mojom::VideoConferenceMediaAppInfoPtr expected_media_app_info =
        crosapi::mojom::VideoConferenceMediaAppInfo::New(
            /*id=*/info0->id,
            /*last_activity_time=*/info0->last_activity_time,
            /*is_capturing_camera=*/true,
            /*is_capturing_microphone=*/true,
            /*is_capturing_screen=*/false,
            /*title=*/base::UTF8ToUTF16(std::string(kPluginVmId)),
            /*url=*/std::nullopt,
            /*app_type=*/crosapi::mojom::VideoConferenceAppType::kPluginVm);

    EXPECT_TRUE(media_app_info[0].Equals(expected_media_app_info));
  }
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAshfeatureClientTest,
                       HandleMediaUsageUpdate) {
  // Set initial permissions.
  UpdateAppPermision(kCrostiniVmId, /*has_camera_permission=*/false,
                     /*has_microphone_permission=*/true);
  UpdateAppPermision(kPluginVmId, /*has_camera_permission=*/true,
                     /*has_microphone_permission=*/false);

  // All state should be false initially.
  VideoConferenceMediaState state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_FALSE(state.has_media_app);
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.has_microphone_permission);
  EXPECT_FALSE(state.is_capturing_camera);
  EXPECT_FALSE(state.is_capturing_microphone);
  EXPECT_FALSE(state.is_capturing_screen);

  // Notifying Crostini is using Mic.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kCrostiniVm,
      VmCameraMicManager::DeviceType::kMic, true);

  // State should have update for microphone from CrostiniVm.
  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_media_app);
  EXPECT_TRUE(state.has_microphone_permission);
  EXPECT_TRUE(state.is_capturing_microphone);
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.is_capturing_camera);

  // Notifying PluginVm is using Mic.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kPluginVm,
      VmCameraMicManager::DeviceType::kMic, true);

  // Extra permission obtained from PluginVm.
  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_camera_permission);
  EXPECT_FALSE(state.is_capturing_camera);

  // Notifying PluginVm is using Camera.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kPluginVm,
      VmCameraMicManager::DeviceType::kCamera, true);

  // Expecting camera capturing from PluginVm.
  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_TRUE(state.has_camera_permission);
  EXPECT_TRUE(state.is_capturing_camera);

  // Notifying Stopping accessing from PluginVm.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kPluginVm,
      VmCameraMicManager::DeviceType::kMic, false);
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kPluginVm,
      VmCameraMicManager::DeviceType::kCamera, false);

  // Camera permission and capturing should be gone.
  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.is_capturing_camera);

  // Notifying Stopping accessing from Crostini.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kCrostiniVm,
      VmCameraMicManager::DeviceType::kMic, false);

  state = GetMediaStateInVideoConferenceManagerAsh();
  EXPECT_FALSE(state.has_media_app);
  EXPECT_FALSE(state.has_camera_permission);
  EXPECT_FALSE(state.has_microphone_permission);
  EXPECT_FALSE(state.is_capturing_camera);
  EXPECT_FALSE(state.is_capturing_microphone);
  EXPECT_FALSE(state.is_capturing_screen);
}

IN_PROC_BROWSER_TEST_F(VideoConferenceAshfeatureClientTest,
                       HandleDeviceUsedWhileDisabled) {
  // Notify disabling state of camera and microphone from
  // video_conference_manager_ash.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->SetSystemMediaDeviceStatus(
          crosapi::mojom::VideoConferenceMediaDevice::kCamera,
          /*disabled=*/true);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->SetSystemMediaDeviceStatus(
          crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
          /*disabled=*/true);

  FakeVideoConferenceTrayController* fake_try_controller =
      static_cast<FakeVideoConferenceTrayController*>(
          VideoConferenceTrayController::Get());

  // Notifying Crostini is using Mic.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kCrostiniVm,
      VmCameraMicManager::DeviceType::kMic, true);

  // One UsedWhileDisabled call should be sent to
  // FakeVideoConferenceTrayController.
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            1u);
  EXPECT_THAT(
      fake_try_controller->device_used_while_disabled_records().back(),
      testing::Pair(crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
                    base::UTF8ToUTF16(std::string(kCrostiniVmId))));

  // Notifying PluginVm is using Camera.
  VideoConferenceAshFeatureClient::Get()->OnVmDeviceUpdated(
      VmCameraMicManager::VmType::kPluginVm,
      VmCameraMicManager::DeviceType::kCamera, true);

  // Another UsedWhileDisabled call should be sent to
  // FakeVideoConferenceTrayController.
  ASSERT_EQ(fake_try_controller->device_used_while_disabled_records().size(),
            2u);
  EXPECT_THAT(fake_try_controller->device_used_while_disabled_records().back(),
              testing::Pair(crosapi::mojom::VideoConferenceMediaDevice::kCamera,
                            base::UTF8ToUTF16(std::string(kPluginVmId))));
}

}  // namespace ash
