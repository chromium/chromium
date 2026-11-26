// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "content/public/browser/desktop_media_id.h"

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
  // The native picker does not require TCC, as macOS considers the user's
  // direct interaction with the OS as conferring one-time permission.
  return !ShouldUseSCContentSharingPicker();
}

bool NeedsSystemScreenCapturePermission(
    const content::DesktopMediaID& media_id) {
  if (!ScreenCaptureNeedsSystemLevelPermissions()) {
    return false;
  }
  // Do not check screen capture permissions when `window_id` is populated. The
  // presence of `window_id` indicates the window to be captured is a Chromium
  // window which will be captured internally without macOS screen capture APIs.
  return media_id.type == content::DesktopMediaID::TYPE_SCREEN ||
         (media_id.type == content::DesktopMediaID::TYPE_WINDOW &&
          !media_id.window_id);
}

}  // namespace system_media_permissions
