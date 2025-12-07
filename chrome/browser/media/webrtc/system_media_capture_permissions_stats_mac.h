// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions for handling stats for system media permissions (camera,
// microphone).

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_STATS_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_STATS_MAC_H_

#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"

class PrefRegistrySimple;

namespace system_media_permissions {

// Registers preferences used for system media permissions stats.
void RegisterSystemMediaPermissionStatesPrefs(PrefRegistrySimple* registry);

// Logs stats for system media permissions. Called once per browser session, at
// browser start.
void LogSystemMediaPermissionsStartupStats();

// Called when a system permission goes from "not determined" to another state.
// The new permission is logged as startup state.
void SystemAudioCapturePermissionDetermined(
    system_permission_settings::SystemPermission permission);
void SystemVideoCapturePermissionDetermined(
    system_permission_settings::SystemPermission permission);

// Adds a sample of the passed in permission to the screen capture metric.
// Called when the screen capture permission is checked.
void LogSystemScreenCapturePermission(bool allowed);

// Called when a system permission was requested but was blocked. Information
// stored is later used when logging stats at startup.
void SystemAudioCapturePermissionBlocked();
void SystemVideoCapturePermissionBlocked();

}  // namespace system_media_permissions

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_STATS_MAC_H_
