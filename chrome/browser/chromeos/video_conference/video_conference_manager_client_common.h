// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_CLIENT_COMMON_H_
#define CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_CLIENT_COMMON_H_

#include <array>
#include <string>

#include "base/time/time.h"
#include "base/unguessable_token.h"

namespace video_conference {

// AppIds that we want to skip tracking.
extern const char* kSkipAppIds[3];

// Returns whether we should skip the contents for tracking.
bool ShouldSkipId(const std::string& id);

// Struct holding the granted status of media device permissions used by
// videoconferencing apps.
struct VideoConferencePermissions {
  bool has_camera_permission = false;
  bool has_microphone_permission = false;
};

// Struct holding state relevant to a VC web app.
struct VideoConferenceWebAppState {
  const base::UnguessableToken id;
  base::Time last_activity_time;
  bool is_capturing_microphone = false;
  bool is_capturing_camera = false;
  bool is_capturing_screen = false;
  bool is_extension = false;
};

// Video conference media devices.
enum VideoConferenceMediaType {
  kCamera,
  kMicrophone,
  kScreen,
};

}  // namespace video_conference

#endif  // CHROME_BROWSER_CHROMEOS_VIDEO_CONFERENCE_VIDEO_CONFERENCE_MANAGER_CLIENT_COMMON_H_
