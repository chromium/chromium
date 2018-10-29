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
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return "Geolocation";
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return "Notifications";
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return "MidiSysEx";
    case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
      return "DurableStorage";
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      return "AudioCapture";
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      return "VideoCapture";
    case CONTENT_SETTINGS_TYPE_MIDI:
      return "Midi";
    case CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
      return "BackgroundSync";
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return "Flash";
    case CONTENT_SETTINGS_TYPE_SENSORS:
      return "Sensors";
    case CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS:
      return "AccessibilityEvents";
    case CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
      return "ClipboardRead";
    case CONTENT_SETTINGS_TYPE_CLIPBOARD_WRITE:
      return "ClipboardWrite";
    case CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER:
      return "PaymentHandler";
    case CONTENT_SETTINGS_TYPE_BACKGROUND_FETCH:
      return "BackgroundFetch";
    default:
      break;
  }
  NOTREACHED();
  return std::string();
}

PermissionRequestType PermissionUtil::GetRequestType(ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return PermissionRequestType::PERMISSION_GEOLOCATION;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return PermissionRequestType::PERMISSION_NOTIFICATIONS;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return PermissionRequestType::PERMISSION_MIDI_SYSEX;
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      return PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return PermissionRequestType::PERMISSION_FLASH;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
      return PermissionRequestType::PERMISSION_MEDIASTREAM_MIC;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      return PermissionRequestType::PERMISSION_MEDIASTREAM_CAMERA;
    case CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS:
      return PermissionRequestType::PERMISSION_ACCESSIBILITY_EVENTS;
    case CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
      return PermissionRequestType::PERMISSION_CLIPBOARD_READ;
    case CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER:
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
  if (type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    *out = PermissionType::GEOLOCATION;
  } else if (type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    *out = PermissionType::NOTIFICATIONS;
  } else if (type == CONTENT_SETTINGS_TYPE_MIDI) {
    *out = PermissionType::MIDI;
  } else if (type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    *out = PermissionType::MIDI_SYSEX;
  } else if (type == CONTENT_SETTINGS_TYPE_DURABLE_STORAGE) {
    *out = PermissionType::DURABLE_STORAGE;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    *out = PermissionType::VIDEO_CAPTURE;
  } else if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) {
    *out = PermissionType::AUDIO_CAPTURE;
  } else if (type == CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC) {
    *out = PermissionType::BACKGROUND_SYNC;
  } else if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    *out = PermissionType::FLASH;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  } else if (type == CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER) {
    *out = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
#endif
  } else if (type == CONTENT_SETTINGS_TYPE_SENSORS) {
    *out = PermissionType::SENSORS;
  } else if (type == CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS) {
    *out = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (type == CONTENT_SETTINGS_TYPE_CLIPBOARD_READ) {
    *out = PermissionType::CLIPBOARD_READ;
  } else if (type == CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER) {
    *out = PermissionType::PAYMENT_HANDLER;
  } else if (type == CONTENT_SETTINGS_TYPE_BACKGROUND_FETCH) {
    *out = PermissionType::BACKGROUND_FETCH;
  } else {
    return false;
  }
  return true;
}

bool PermissionUtil::IsPermission(ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
    case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC:
    case CONTENT_SETTINGS_TYPE_PLUGINS:
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
#endif
    case CONTENT_SETTINGS_TYPE_SENSORS:
    case CONTENT_SETTINGS_TYPE_ACCESSIBILITY_EVENTS:
    case CONTENT_SETTINGS_TYPE_CLIPBOARD_READ:
    case CONTENT_SETTINGS_TYPE_PAYMENT_HANDLER:
    case CONTENT_SETTINGS_TYPE_BACKGROUND_FETCH:
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
