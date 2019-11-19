// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_util.h"

#include "build/build_config.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_type.h"

using content::PermissionType;

// The returned strings must match any Field Trial configs for the Permissions
// kill switch e.g. Permissions.Action.Geolocation etc..
std::string PermissionUtil::GetPermissionString(
    ContentSettingsType content_type) {
  switch (content_type) {
    case ContentSettingsType::GEOLOCATION:
      return "Geolocation";
    case ContentSettingsType::NOTIFICATIONS:
      return "Notifications";
    case ContentSettingsType::MIDI_SYSEX:
      return "MidiSysEx";
    case ContentSettingsType::DURABLE_STORAGE:
      return "DurableStorage";
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case ContentSettingsType::MEDIASTREAM_MIC:
      return "AudioCapture";
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case ContentSettingsType::MIDI:
      return "Midi";
    case ContentSettingsType::BACKGROUND_SYNC:
      return "BackgroundSync";
    case ContentSettingsType::PLUGINS:
      return "Flash";
    case ContentSettingsType::SENSORS:
      return "Sensors";
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return "AccessibilityEvents";
    case ContentSettingsType::CLIPBOARD_READ:
      return "ClipboardRead";
    case ContentSettingsType::CLIPBOARD_WRITE:
      return "ClipboardWrite";
    case ContentSettingsType::PAYMENT_HANDLER:
      return "PaymentHandler";
    case ContentSettingsType::BACKGROUND_FETCH:
      return "BackgroundFetch";
    case ContentSettingsType::IDLE_DETECTION:
      return "IdleDetection";
    case ContentSettingsType::PERIODIC_BACKGROUND_SYNC:
      return "PeriodicBackgroundSync";
    case ContentSettingsType::WAKE_LOCK_SCREEN:
      return "WakeLockScreen";
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
      return "WakeLockSystem";
    case ContentSettingsType::NFC:
      return "NFC";
    default:
      break;
  }
  NOTREACHED();
  return std::string();
}

PermissionRequestType PermissionUtil::GetRequestType(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::GEOLOCATION:
      return PermissionRequestType::PERMISSION_GEOLOCATION;
    case ContentSettingsType::NOTIFICATIONS:
      return PermissionRequestType::PERMISSION_NOTIFICATIONS;
    case ContentSettingsType::MIDI_SYSEX:
      return PermissionRequestType::PERMISSION_MIDI_SYSEX;
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      return PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER;
    case ContentSettingsType::PLUGINS:
      return PermissionRequestType::PERMISSION_FLASH;
    case ContentSettingsType::MEDIASTREAM_MIC:
      return PermissionRequestType::PERMISSION_MEDIASTREAM_MIC;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      return PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA;
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
      return PermissionRequestType::PERMISSION_ACCESSIBILITY_EVENTS;
    case ContentSettingsType::CLIPBOARD_READ:
      return PermissionRequestType::PERMISSION_CLIPBOARD_READ;
    case ContentSettingsType::PAYMENT_HANDLER:
      return PermissionRequestType::PERMISSION_PAYMENT_HANDLER;
    default:
      NOTREACHED();
      return PermissionRequestType::UNKNOWN;
  }
}

PermissionRequestGestureType PermissionUtil::GetGestureType(bool user_gesture) {
  return user_gesture ? PermissionRequestGestureType::GESTURE
                      : PermissionRequestGestureType::NO_GESTURE;
}

bool PermissionUtil::GetPermissionType(ContentSettingsType type,
                                       PermissionType* out) {
  if (type == ContentSettingsType::GEOLOCATION) {
    *out = PermissionType::GEOLOCATION;
  } else if (type == ContentSettingsType::NOTIFICATIONS) {
    *out = PermissionType::NOTIFICATIONS;
  } else if (type == ContentSettingsType::MIDI) {
    *out = PermissionType::MIDI;
  } else if (type == ContentSettingsType::MIDI_SYSEX) {
    *out = PermissionType::MIDI_SYSEX;
  } else if (type == ContentSettingsType::DURABLE_STORAGE) {
    *out = PermissionType::DURABLE_STORAGE;
  } else if (type == ContentSettingsType::MEDIASTREAM_CAMERA) {
    *out = PermissionType::VIDEO_CAPTURE;
  } else if (type == ContentSettingsType::MEDIASTREAM_MIC) {
    *out = PermissionType::AUDIO_CAPTURE;
  } else if (type == ContentSettingsType::BACKGROUND_SYNC) {
    *out = PermissionType::BACKGROUND_SYNC;
  } else if (type == ContentSettingsType::PLUGINS) {
    *out = PermissionType::FLASH;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  } else if (type == ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER) {
    *out = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
#endif
  } else if (type == ContentSettingsType::SENSORS) {
    *out = PermissionType::SENSORS;
  } else if (type == ContentSettingsType::ACCESSIBILITY_EVENTS) {
    *out = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (type == ContentSettingsType::CLIPBOARD_READ) {
    *out = PermissionType::CLIPBOARD_READ;
  } else if (type == ContentSettingsType::PAYMENT_HANDLER) {
    *out = PermissionType::PAYMENT_HANDLER;
  } else if (type == ContentSettingsType::BACKGROUND_FETCH) {
    *out = PermissionType::BACKGROUND_FETCH;
  } else if (type == ContentSettingsType::PERIODIC_BACKGROUND_SYNC) {
    *out = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (type == ContentSettingsType::WAKE_LOCK_SCREEN) {
    *out = PermissionType::WAKE_LOCK_SCREEN;
  } else if (type == ContentSettingsType::WAKE_LOCK_SYSTEM) {
    *out = PermissionType::WAKE_LOCK_SYSTEM;
  } else if (type == ContentSettingsType::NFC) {
    *out = PermissionType::NFC;
  } else {
    return false;
  }
  return true;
}

bool PermissionUtil::IsPermission(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::GEOLOCATION:
    case ContentSettingsType::NOTIFICATIONS:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::DURABLE_STORAGE:
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::MEDIASTREAM_MIC:
    case ContentSettingsType::BACKGROUND_SYNC:
    case ContentSettingsType::PLUGINS:
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
#endif
    case ContentSettingsType::SENSORS:
    case ContentSettingsType::ACCESSIBILITY_EVENTS:
    case ContentSettingsType::CLIPBOARD_READ:
    case ContentSettingsType::PAYMENT_HANDLER:
    case ContentSettingsType::BACKGROUND_FETCH:
    case ContentSettingsType::PERIODIC_BACKGROUND_SYNC:
    case ContentSettingsType::WAKE_LOCK_SCREEN:
    case ContentSettingsType::WAKE_LOCK_SYSTEM:
#if defined(OS_ANDROID)
    case ContentSettingsType::NFC:
#endif
      return true;
    default:
      return false;
  }
}

PermissionUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    Profile* profile,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : profile_(profile),
      primary_url_(primary_url),
      secondary_url_(secondary_url),
      content_type_(content_type),
      source_ui_(source_ui) {
  if (!primary_url_.is_valid() ||
      (!secondary_url_.is_valid() && !secondary_url_.is_empty())) {
    is_initially_allowed_ = false;
    return;
  }
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  ContentSetting initial_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_, std::string());
  is_initially_allowed_ = initial_content_setting == CONTENT_SETTING_ALLOW;
}

PermissionUtil::ScopedRevocationReporter::ScopedRevocationReporter(
    Profile* profile,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    PermissionSourceUI source_ui)
    : ScopedRevocationReporter(
          profile,
          GURL(primary_pattern.ToString()),
          GURL((secondary_pattern == ContentSettingsPattern::Wildcard())
                   ? primary_pattern.ToString()
                   : secondary_pattern.ToString()),
          content_type,
          source_ui) {}

PermissionUtil::ScopedRevocationReporter::~ScopedRevocationReporter() {
  if (!is_initially_allowed_)
    return;
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  ContentSetting final_content_setting = settings_map->GetContentSetting(
      primary_url_, secondary_url_, content_type_, std::string());
  if (final_content_setting != CONTENT_SETTING_ALLOW) {
    // PermissionUmaUtil takes origins, even though they're typed as GURL.
    GURL requesting_origin = primary_url_.GetOrigin();
    PermissionUmaUtil::PermissionRevoked(content_type_, source_ui_,
                                         requesting_origin, profile_);
  }
}
