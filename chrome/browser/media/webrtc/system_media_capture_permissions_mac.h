// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_

#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"

namespace content {
struct DesktopMediaID;
}  // namespace content

namespace system_media_permissions {

// Returns the system permission to capture the screen.
system_permission_settings::SystemPermission
CheckSystemScreenCapturePermission();

// Returns whether a system-level permission is needed to capture the screen.
bool ScreenCaptureNeedsSystemLevelPermissions();

// Returns whether capturing `media_id` requires a system-level screen capture
// permission check (assuming the source was selected via a picker that uses
// `SCContentSharingPicker` when available).
bool NeedsSystemScreenCapturePermission(
    const content::DesktopMediaID& media_id);

}  // namespace system_media_permissions

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_MAC_H_
