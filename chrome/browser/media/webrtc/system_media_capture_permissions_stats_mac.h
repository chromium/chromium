// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions for handling stats for system media permissions (camera,
// microphone).

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_STATS_MAC_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_STATS_MAC_H_

#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"

class PrefRegistrySimple;

namespace system_media_permissions {

// Registers preferences used for system media permissions stats.
void RegisterSystemMediaPermissionStatesPrefs(PrefRegistrySimple* registry);

// Logs stats for system media permissions. Called once per browser session, at
// browser start.
void LogSystemMediaPermissionsStartupStats();

// Called when a system permission goes from "not determined" to another state.
// The new permission is logged as startup state.
void SystemAudioCapturePermissionDetermined(SystemPermission permission);
void SystemVideoCapturePermissionDetermined(SystemPermission permission);

// Called when a system permission was requested but was blocked. Information
// stored is later used when logging stats at startup.
void SystemAudioCapturePermissionBlocked();
void SystemVideoCapturePermissionBlocked();

}  // namespace system_media_permissions

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SYSTEM_MEDIA_CAPTURE_PERMISSIONS_STATS_MAC_H_
