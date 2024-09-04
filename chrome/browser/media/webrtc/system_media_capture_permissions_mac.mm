// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"

namespace system_media_permissions {

using ::system_permission_settings::SystemPermission;

SystemPermission CheckSystemScreenCapturePermission() {
  SystemPermission system_permission =
      system_permission_settings::CheckSystemScreenCapturePermission();

  LogSystemScreenCapturePermission(system_permission ==
                                   SystemPermission::kAllowed);

  return system_permission;
}

}  // namespace system_media_permissions
