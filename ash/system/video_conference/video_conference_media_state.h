// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MEDIA_STATE_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MEDIA_STATE_H_

namespace ash {

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

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MEDIA_STATE_H_
