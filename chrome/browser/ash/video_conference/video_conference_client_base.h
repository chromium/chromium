// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_BASE_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_BASE_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash {

class VideoConferenceManagerAsh;

// Base class for Video Conference Manager clients in Ash-chrome. This class
// provides a common implementation for clients that interact with the
// VideoConferenceManagerAsh.
class VideoConferenceClientBase
    : public crosapi::mojom::VideoConferenceManagerClient {
 protected:
  explicit VideoConferenceClientBase(
      VideoConferenceManagerAsh* video_conference_manager_ash);
  ~VideoConferenceClientBase() override;

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

  // crosapi::mojom::VideoConferenceManagerClient:
  void GetMediaApps(GetMediaAppsCallback callback) override;
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool enabled,
      SetSystemMediaDeviceStatusCallback callback) override;
  void StopAllScreenShare() override;

  // Calculates a new `crosapi::mojom::VideoConferenceMediaUsageStatus` from all
  // current VC apps and notifies the manager if a field has changed.
  void HandleMediaUsageUpdate();

  // Returns the name of the app with `app_id`. This name is used for display
  // in notifications and other UI surfaces.
  virtual std::string GetAppName(const AppIdString& app_id) = 0;
  // Returns the current camera and microphone permission status for the app
  // with `app_id`.
  virtual VideoConferencePermissions GetAppPermission(
      const AppIdString& app_id) = 0;
  // Returns the type of the app with `app_id`. This is used by the manager
  // to distinguish between different kinds of applications.
  virtual apps::AppType GetAppType(const AppIdString& app_id) = 0;

  bool camera_system_enabled_{true};
  bool microphone_system_enabled_{true};

  // This records a list of AppState; each represents a video conference app.
  std::map<AppIdString, AppState> id_to_app_state_;

  // Unique id associated with this client. It is used by the VcManager to
  // identify clients.
  const base::UnguessableToken client_id_;

  // Current status_ aggregated from all apps in `id_to_app_state_`.
  crosapi::mojom::VideoConferenceMediaUsageStatusPtr status_;

  const raw_ref<VideoConferenceManagerAsh> video_conference_manager_ash_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_BASE_H_
