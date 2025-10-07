// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_ASH_FEATURE_CLIENT_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_ASH_FEATURE_CLIENT_H_

#include "base/unguessable_token.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_client_base.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"

namespace ash {

class VideoConferenceManagerAsh;

// VideoConferenceAshFeatureClient is a client class for CrOS
// videoconferencing.
// It covers the individual features that are not covered by the browser or the
// AppService. This includes: Vms (Crostini, PluginVm, Borealis), screen
// capturer, diction etc.
class VideoConferenceAshFeatureClient : public VideoConferenceClientBase {
 public:
  // The passed `video_conference_manager_ash` must outlive this instance.
  explicit VideoConferenceAshFeatureClient(
      VideoConferenceManagerAsh* video_conference_manager_ash);

  VideoConferenceAshFeatureClient(const VideoConferenceAshFeatureClient&) =
      delete;
  VideoConferenceAshFeatureClient& operator=(
      const VideoConferenceAshFeatureClient&) = delete;

  ~VideoConferenceAshFeatureClient() override;

  void ReturnToApp(const base::UnguessableToken& token,
                   ReturnToAppCallback callback) override;

  // Called when VmCameraMicManager change Camera/Mic accessing state.
  void OnVmDeviceUpdated(VmCameraMicManager::VmType vm_type,
                         VmCameraMicManager::DeviceType device_type,
                         bool is_capturing);

  // Returns current VideoConferenceAshFeatureClient.
  static VideoConferenceAshFeatureClient* Get();

 protected:
  // VideoConferenceClientBase overrides.
  std::string GetAppName(const AppIdString& app_id) override;
  VideoConferencePermissions GetAppPermission(
      const AppIdString& app_id) override;
  apps::AppType GetAppType(const AppIdString& app_id) override;

 private:
  friend class VideoConferenceAshfeatureClientTest;

  // Returns AppState of `app_id`; adds if doesn't exist yet.
  AppState& GetOrAddAppState(const AppIdString& app_id);

  // Removes `app_id` from `id_to_app_state_` if there is no running instance
  // for it.
  void MaybeRemoveApp(const AppIdString& app_id);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_ASH_FEATURE_CLIENT_H_
