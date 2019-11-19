// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace system_media_permissions {

namespace {

const char kSystemPermissionMicFirstBlockedTimePref[] =
    "system_permission.mic.first_blocked_time";
const char kSystemPermissionMicLastBlockedTimePref[] =
    "system_permission.mic.last_blocked_time";
const char kSystemPermissionCameraFirstBlockedTimePref[] =
    "system_permission.camera.first_blocked_time";
const char kSystemPermissionCameraLastBlockedTimePref[] =
    "system_permission.camera.last_blocked_time";

void LogStartupMicSystemPermission(SystemPermission permission) {
  base::UmaHistogramEnumeration(
      "Media.Audio.Capture.Mac.MicSystemPermission.Startup", permission);
}

void LogStartupCameraSystemPermission(SystemPermission permission) {
  base::UmaHistogramEnumeration(
      "Media.Video.Capture.Mac.CameraSystemPermission.Startup", permission);
}

void MaybeLogAdditionalMicSystemPermissionStats(SystemPermission permission) {
  PrefService* prefs = g_browser_process->local_state();

  if (!prefs->HasPrefPath(kSystemPermissionMicFirstBlockedTimePref)) {
    DCHECK(!prefs->HasPrefPath(kSystemPermissionMicLastBlockedTimePref));
    return;
  }

  // A pref exists, so there was a failure accessing the mic due to blocked
  // system permission before the last restart. Log additional stats.

  DCHECK(prefs->HasPrefPath(kSystemPermissionMicLastBlockedTimePref));
  base::UmaHistogramEnumeration("Media.Audio.Capture.Mac.MicSystemPermission."
                                "StartupAfterFailure",
                                permission);

  // If the state has changed to allowed, log the time it took since first
  // and last failure before restart. Check for positive time delta, since
  // the system clock may change at any time.
  if (permission == SystemPermission::kAllowed) {
    base::Time stored_time =
        prefs->GetTime(kSystemPermissionMicFirstBlockedTimePref);
    base::TimeDelta time_delta = base::Time::Now() - stored_time;
    if (time_delta > base::TimeDelta()) {
      base::UmaHistogramCustomTimes(
          "Media.Audio.Capture.Mac.MicSystemPermission."
          "FixedTime.SinceFirstFailure",
          time_delta, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromHours(1), 50);
    }

    stored_time = prefs->GetTime(kSystemPermissionMicLastBlockedTimePref);
    time_delta = base::Time::Now() - stored_time;
    if (time_delta > base::TimeDelta()) {
      base::UmaHistogramCustomTimes(
          "Media.Audio.Capture.Mac.MicSystemPermission."
          "FixedTime.SinceLastFailure",
          time_delta, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromHours(1), 50);
    }
  }

  prefs->ClearPref(kSystemPermissionMicFirstBlockedTimePref);
  prefs->ClearPref(kSystemPermissionMicLastBlockedTimePref);
}

void MaybeLogAdditionalCameraSystemPermissionStats(
    SystemPermission permission) {
  PrefService* prefs = g_browser_process->local_state();

  if (!prefs->HasPrefPath(kSystemPermissionCameraFirstBlockedTimePref)) {
    DCHECK(!prefs->HasPrefPath(kSystemPermissionCameraLastBlockedTimePref));
    return;
  }

  // A pref exists, so there was a failure accessing the camera due to blocked
  // system permission before the last restart. Log additional stats.

  DCHECK(prefs->HasPrefPath(kSystemPermissionCameraLastBlockedTimePref));
  base::UmaHistogramEnumeration(
      "Media.Video.Capture.Mac.CameraSystemPermission."
      "StartupAfterFailure",
      permission);

  // If the state has changed to allowed, log the time it took since first
  // and last failure before restart. Check for positive time delta, since
  // the system clock may change at any time.
  if (permission == SystemPermission::kAllowed) {
    base::Time stored_time =
        prefs->GetTime(kSystemPermissionCameraFirstBlockedTimePref);
    base::TimeDelta time_delta = base::Time::Now() - stored_time;
    if (time_delta > base::TimeDelta()) {
      base::UmaHistogramCustomTimes(
          "Media.Video.Capture.Mac.CameraSystemPermission.FixedTime."
          "SinceFirstFailure",
          time_delta, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromHours(1), 50);
    }

    stored_time = prefs->GetTime(kSystemPermissionCameraLastBlockedTimePref);
    time_delta = base::Time::Now() - stored_time;
    if (time_delta > base::TimeDelta()) {
      base::UmaHistogramCustomTimes(
          "Media.Video.Capture.Mac.CameraSystemPermission.FixedTime."
          "SinceLastFailure",
          time_delta, base::TimeDelta::FromSeconds(1),
          base::TimeDelta::FromHours(1), 50);
    }
  }

  prefs->ClearPref(kSystemPermissionCameraFirstBlockedTimePref);
  prefs->ClearPref(kSystemPermissionCameraLastBlockedTimePref);
}

}  // namespace

void RegisterSystemMediaPermissionStatesPrefs(PrefRegistrySimple* registry) {
  if (@available(macOS 10.14, *)) {
    registry->RegisterTimePref(kSystemPermissionMicFirstBlockedTimePref,
                               base::Time());
    registry->RegisterTimePref(kSystemPermissionMicLastBlockedTimePref,
                               base::Time());
    registry->RegisterTimePref(kSystemPermissionCameraFirstBlockedTimePref,
                               base::Time());
    registry->RegisterTimePref(kSystemPermissionCameraLastBlockedTimePref,
                               base::Time());
  }
}

void LogSystemMediaPermissionsStartupStats() {
  if (@available(macOS 10.14, *)) {
    const SystemPermission audio_permission =
        CheckSystemAudioCapturePermission();
    LogStartupMicSystemPermission(audio_permission);
    MaybeLogAdditionalMicSystemPermissionStats(audio_permission);

    const SystemPermission video_permission =
        CheckSystemVideoCapturePermission();
    LogStartupCameraSystemPermission(video_permission);
    MaybeLogAdditionalCameraSystemPermissionStats(video_permission);
  }  // (@available(macOS 10.14, *))
}

void SystemAudioCapturePermissionDetermined(SystemPermission permission) {
  if (@available(macOS 10.14, *)) {
    DCHECK_NE(permission, SystemPermission::kNotDetermined);
    LogStartupMicSystemPermission(permission);
  }
}

void SystemVideoCapturePermissionDetermined(SystemPermission permission) {
  if (@available(macOS 10.14, *)) {
    DCHECK_NE(permission, SystemPermission::kNotDetermined);
    LogStartupCameraSystemPermission(permission);
  }
}

void SystemAudioCapturePermissionBlocked() {
  if (@available(macOS 10.14, *)) {
    PrefService* prefs = g_browser_process->local_state();
    if (!prefs->HasPrefPath(kSystemPermissionMicFirstBlockedTimePref)) {
      prefs->SetTime(kSystemPermissionMicFirstBlockedTimePref,
                     base::Time::Now());
    }
    prefs->SetTime(kSystemPermissionMicLastBlockedTimePref, base::Time::Now());
  }
}

void SystemVideoCapturePermissionBlocked() {
  if (@available(macOS 10.14, *)) {
    PrefService* prefs = g_browser_process->local_state();
    if (!prefs->HasPrefPath(kSystemPermissionCameraFirstBlockedTimePref)) {
      prefs->SetTime(kSystemPermissionCameraFirstBlockedTimePref,
                     base::Time::Now());
    }
    prefs->SetTime(kSystemPermissionCameraLastBlockedTimePref,
                   base::Time::Now());
  }
}

}  // namespace system_media_permissions
