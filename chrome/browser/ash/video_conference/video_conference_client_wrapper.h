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

// TODO(crbug.com/365741912, crbug.com/365902693): Remove this wrapper in a
// later CL now that we use the C++ client interface directly.
//
// |VideoConferenceClientWrapper| adds a VideoConferenceMediaState member
// variable for use by VideoConferenceManagerAsh.
class VideoConferenceClientWrapper {
 public:
  explicit VideoConferenceClientWrapper(VideoConferenceManagerClient* client);

  ~VideoConferenceClientWrapper();

  void GetMediaApps(
      VideoConferenceManagerClient::GetMediaAppsCallback callback);
  void ReturnToApp(const base::UnguessableToken& id,
                   VideoConferenceManagerClient::ReturnToAppCallback callback);
  void SetSystemMediaDeviceStatus(
      VideoConferenceMediaDevice device,
      bool enabled,
      VideoConferenceManagerClient::SetSystemMediaDeviceStatusCallback
          callback);

  VideoConferenceMediaState& state();

 private:
  VideoConferenceMediaState state_;
  raw_ptr<VideoConferenceManagerClient> cpp_client_{nullptr};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_VIDEO_CONFERENCE_VIDEO_CONFERENCE_CLIENT_WRAPPER_H_
