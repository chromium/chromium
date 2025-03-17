// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_
#define CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_

#include <cstdint>

#include "ash/system/video_conference/video_conference_common.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

// |VideoConferenceClientWrapper| adds a VideoConferenceMediaState member
// variable for use by VideoConferenceManagerAsh.
class VideoConferenceClientWrapper {
 public:
  explicit VideoConferenceClientWrapper(
      crosapi::mojom::VideoConferenceManagerClient* client);

  ~VideoConferenceClientWrapper();

  // API mirroring the one in `crosapi::mojom::VideoConferenceManagerClient`
  void GetMediaApps(
      crosapi::mojom::VideoConferenceManagerClient::GetMediaAppsCallback
          callback);
  void ReturnToApp(
      const base::UnguessableToken& id,
      crosapi::mojom::VideoConferenceManagerClient::ReturnToAppCallback
          callback);
  void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled,
      crosapi::mojom::VideoConferenceManagerClient::
          SetSystemMediaDeviceStatusCallback callback);

  void StopAllScreenShare();

  VideoConferenceMediaState& state();

 private:
  VideoConferenceMediaState state_;
  raw_ptr<crosapi::mojom::VideoConferenceManagerClient> cpp_client_{nullptr};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_
