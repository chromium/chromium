// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "media/base/media_switches.h"

namespace system_media_permissions {

using ::system_permission_settings::SystemPermission;

SystemPermission CheckSystemScreenCapturePermission() {
  SystemPermission system_permission =
      system_permission_settings::CheckSystemScreenCapturePermission();

  LogSystemScreenCapturePermission(system_permission ==
                                   SystemPermission::kAllowed);

  return system_permission;
}

bool ScreenCaptureNeedsSystemLevelPermissions() {
  if (@available(macOS 15, *)) {
    // The native picker does not require TCC, as macOS considers the user's
    // direct interaction with the OS as conferring one-time permission.
    return !base::FeatureList::IsEnabled(media::kUseSCContentSharingPicker);
  }
  return true;
}

}  // namespace system_media_permissions
