// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_COMMON_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_COMMON_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"

namespace ash {

constexpr int kVideoConferenceBubbleHorizontalPadding = 16;

const int kReturnToAppIconSize = 20;

// The duration for the gradient animation on the Image and Create with AI
// buttons.
const base::TimeDelta kGradientAnimationDuration = base::Milliseconds(3120);

// This struct provides aggregated attributes of media apps
// from one or more clients.
struct VideoConferenceMediaState {
  // At least one media app is running on the client(s).
  bool has_media_app = false;
  // At least one media app has camera permission on the client(s).
  bool has_camera_permission = false;
  // At least one media app has microphone permission on the client(s).
  bool has_microphone_permission = false;
  // At least one media app is capturing the camera on the client(s).
  bool is_capturing_camera = false;
  // At least one media app is capturing the microphone on the client(s).
  bool is_capturing_microphone = false;
  // At least one media app is capturing the screen on the client(s).
  bool is_capturing_screen = false;
};

// This class defines the public interfaces of VideoConferenceManagerAsh exposed
// to VideoConferenceTrayController. Although these public functions look
// identical to VideoConferenceManagerClient, we should not use
// VideoConferenceManagerClient here; because they represent different concepts.
// The signal will be passed from VideoConferenceTrayController to
// VideoConferenceManagerAsh to VideoConferenceManagerClient.
class VideoConferenceManagerBase {
 public:
  using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;
  // Gets all media apps from VideoConferenceManagerAsh and runs the callback on
  // that.
  virtual void GetMediaApps(base::OnceCallback<void(MediaApps)>) = 0;

  // Calls VideoConferenceManagerAsh to return to App identified by `id`.
  virtual void ReturnToApp(const base::UnguessableToken& id) = 0;

  // Sets whether |device| is disabled at the system or hardware level.
  virtual void SetSystemMediaDeviceStatus(
      crosapi::mojom::VideoConferenceMediaDevice device,
      bool disabled) = 0;

  // Stops all screen sharing.
  virtual void StopAllScreenShare() = 0;

  // Called when CreateBackgroundImage button is clicked on.
  virtual void CreateBackgroundImage() = 0;

  virtual ~VideoConferenceManagerBase() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_COMMON_H_
