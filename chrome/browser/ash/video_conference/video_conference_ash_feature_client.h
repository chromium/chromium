// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_ASH_FEATURE_CLIENT_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_ASH_FEATURE_CLIENT_H_

#include "base/unguessable_token.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"

namespace ash {

// VideoConferenceAshFeatureClient is a client class for CrOS
// videoconferencing.
// It covers the individual features that are not covered by the browser or the
// AppService. This includes: Vms (Crostini, PluginVm, Borealis), screen
// capturer, diction etc.
class VideoConferenceAshFeatureClient
    : public crosapi::mojom::VideoConferenceManagerClient {
 public:
  using AppIdString = std::string;
  using VideoConferencePermissions =
      video_conference::VideoConferencePermissions;

  // AppState records information that is required for VideoConferenceManagerAsh
  // to show correct icons.
  struct AppState {
    // Used for uniquely identifying an App in VideoConferenceManagerAsh.
    base::UnguessableToken token;
    base::Time last_activity_time;
    bool is_capturing_microphone = false;
    bool is_capturing_camera = false;
  };

  VideoConferenceAshFeatureClient();

  VideoConferenceAshFeatureClient(const VideoConferenceAshFeatureClient&) =
      delete;
  VideoConferenceAshFeatureClient& operator=(
      const VideoConferenceAshFeatureClient&) = delete;

  ~VideoConferenceAshFeatureClient() override;

  // crosapi::mojom::VideoConferenceManagerClient overrides.
  void GetMediaApps(GetMediaAppsCallback callback) override;
  void ReturnToApp(const base::UnguessableToken& token,
                   ReturnToAppCallback callback) override;
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      SetSystemMediaDeviceStatusCallback callback) override;
  void StopAllScreenShare() override;

  // Called when VmCameraMicManager change Camera/Mic accessing state.
  void OnVmDeviceUpdated(VmCameraMicManager::VmType vm_type,
                         VmCameraMicManager::DeviceType device_type,
                         bool is_capturing);

  // Returns current VideoConferenceAshFeatureClient.
  static VideoConferenceAshFeatureClient* Get();

 private:
  friend class VideoConferenceAshfeatureClientTest;

  // Returns the name of the app with `app_id`.
  std::string GetAppName(const AppIdString& app_id);

  // Returns the current camera/microphone permission status for `app_id`.
  VideoConferencePermissions GetAppPermission(const AppIdString& app_id);

  // Returns the crosapi::mojom::VideoConferenceAppType of `app_id`.
  crosapi::mojom::VideoConferenceAppType GetAppType(const AppIdString& app_id);

  // Returns AppState of `app_id`; adds if doesn't exist yet.
  AppState& GetOrAddAppState(const AppIdString& app_id);

  // Removes `app_id` from `id_to_app_state_` if there is no running instance
  // for it.
  void MaybeRemoveApp(const AppIdString& app_id);

  // Calculates a new `crosapi::mojom::VideoConferenceMediaUsageStatus` from all
  // current VC apps and notifies the manager if a field has changed.
  void HandleMediaUsageUpdate();

  // Unique id associated with this client. It is used by the VcManager to
  // identify clients.
  const base::UnguessableToken client_id_;

  // Current status_ aggregated from all apps in `id_to_app_state_`.
  crosapi::mojom::VideoConferenceMediaUsageStatusPtr status_;

  // The following two fields are true if the camera/microphone is system-wide
  // software disabled OR disabled via a hardware switch.
  bool camera_system_disabled_{false};
  bool microphone_system_disabled_{false};

  // This records a list of AppState; each represents a video conference app.
  std::map<AppIdString, AppState> id_to_app_state_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_ASH_FEATURE_CLIENT_H_
