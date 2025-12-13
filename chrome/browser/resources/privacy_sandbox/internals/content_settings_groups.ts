// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContentSettingsType} from './content_settings_types.mojom-webui.js';

/**
 * @fileoverview
 * This file defines the UI groupings for Content Settings. The structure is
 * derived from the groupings in content_settings.mojom and should be kept
 * in sync with the group comments in that file.
 *
 * Be aware that any new content settings you create will not be
 * automatically categorized. By default, they will be added to the "Other"
 * group. If you wish to place them in a different group, you must do so
 * manually.
 */

export const contentSettingGroups:
    Array<{name: string, settings: ContentSettingsType[]}> = [
      {
        name: 'Privacy, Tracking & Security',
        settings: [
          ContentSettingsType.ADS,
          ContentSettingsType.ADS_DATA,
          ContentSettingsType.ANTI_ABUSE,
          ContentSettingsType.COOKIE_CONTROLS_METADATA,
          ContentSettingsType.LEGACY_COOKIE_ACCESS,
          ContentSettingsType.LEGACY_COOKIE_SCOPE,
          ContentSettingsType.PASSWORD_PROTECTION,
          ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
          ContentSettingsType.SAFE_BROWSING_URL_CHECK_DATA,
          ContentSettingsType.SSL_CERT_DECISIONS,
          ContentSettingsType.STORAGE_ACCESS,
          ContentSettingsType.STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
          ContentSettingsType.THIRD_PARTY_STORAGE_PARTITIONING,
          ContentSettingsType.TOP_LEVEL_STORAGE_ACCESS,
          ContentSettingsType.TPCD_HEURISTICS_GRANTS,
          ContentSettingsType.TPCD_METADATA_GRANTS,
          ContentSettingsType.TRACKING_PROTECTION,
        ],
      },
      {
        name: 'Core Content & Scripting',
        settings: [
          ContentSettingsType.AUTOPLAY,
          ContentSettingsType.CLIPBOARD_READ_WRITE,
          ContentSettingsType.CLIPBOARD_SANITIZED_WRITE,
          ContentSettingsType.COOKIES,
          ContentSettingsType.IMAGES,
          ContentSettingsType.JAVASCRIPT,
          ContentSettingsType.JAVASCRIPT_JIT,
          ContentSettingsType.JAVASCRIPT_OPTIMIZER,
          ContentSettingsType.MIXEDSCRIPT,
          ContentSettingsType.POPUPS,
          ContentSettingsType.SOUND,
        ],
      },
      {
        name: 'Hardware Access & Sensors',
        settings: [
          ContentSettingsType.AR,
          ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
          ContentSettingsType.BLUETOOTH_GUARD,
          ContentSettingsType.BLUETOOTH_SCANNING,
          ContentSettingsType.CAMERA_PAN_TILT_ZOOM,
          ContentSettingsType.GEOLOCATION,
          ContentSettingsType.HAND_TRACKING,
          ContentSettingsType.HID_CHOOSER_DATA,
          ContentSettingsType.HID_GUARD,
          ContentSettingsType.KEYBOARD_LOCK,
          ContentSettingsType.MEDIASTREAM_CAMERA,
          ContentSettingsType.MEDIASTREAM_MIC,
          ContentSettingsType.MIDI,
          ContentSettingsType.MIDI_SYSEX,
          ContentSettingsType.NFC,
          ContentSettingsType.POINTER_LOCK,
          ContentSettingsType.SENSORS,
          ContentSettingsType.SERIAL_CHOOSER_DATA,
          ContentSettingsType.SERIAL_GUARD,
          ContentSettingsType.SMART_CARD_DATA,
          ContentSettingsType.SMART_CARD_GUARD,
          ContentSettingsType.SPEAKER_SELECTION,
          ContentSettingsType.USB_CHOOSER_DATA,
          ContentSettingsType.USB_GUARD,
          ContentSettingsType.VR,
        ],
      },
      {
        name: 'File System & Storage',
        settings: [
          ContentSettingsType.DURABLE_STORAGE,
          ContentSettingsType.FILE_SYSTEM_ACCESS_CHOOSER_DATA,
          ContentSettingsType.FILE_SYSTEM_ACCESS_EXTENDED_PERMISSION,
          ContentSettingsType.FILE_SYSTEM_ACCESS_RESTORE_PERMISSION,
          ContentSettingsType.FILE_SYSTEM_LAST_PICKED_DIRECTORY,
          ContentSettingsType.FILE_SYSTEM_READ_GUARD,
          ContentSettingsType.FILE_SYSTEM_WRITE_GUARD,
          ContentSettingsType.LOCAL_FONTS,
        ],
      },
      {
        name: 'Network & Connectivity',
        settings: [
          ContentSettingsType.AUTOMATIC_DOWNLOADS,
          ContentSettingsType.DIRECT_SOCKETS,
          ContentSettingsType.DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS,
          ContentSettingsType.LOCAL_NETWORK_ACCESS,
          ContentSettingsType.PROTOCOL_HANDLERS,
        ],
      },
      {
        name: 'Notifications & Permissions',
        settings: [
          ContentSettingsType.ARE_SUSPICIOUS_NOTIFICATIONS_ALLOWLISTED_BY_USER,
          ContentSettingsType.NOTIFICATIONS,
          ContentSettingsType.NOTIFICATION_INTERACTIONS,
          ContentSettingsType.NOTIFICATION_PERMISSION_REVIEW,
          ContentSettingsType.PERMISSION_AUTOBLOCKER_DATA,
          ContentSettingsType.PERMISSION_AUTOREVOCATION_DATA,
          ContentSettingsType.PERMISSION_ACTIONS_HISTORY,
          ContentSettingsType.REVOKED_ABUSIVE_NOTIFICATION_PERMISSIONS,
          ContentSettingsType.REVOKED_DISRUPTIVE_NOTIFICATION_PERMISSIONS,
          ContentSettingsType.REVOKED_UNUSED_SITE_PERMISSIONS,
          ContentSettingsType.SUSPICIOUS_NOTIFICATION_IDS,
        ],
      },
      {
        name: 'Federated Identity & Sign-In (FedCM)',
        settings: [
          ContentSettingsType.FEDERATED_IDENTITY_API,
          ContentSettingsType.FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
          ContentSettingsType.FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION,
          ContentSettingsType
              .FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
          ContentSettingsType.FEDERATED_IDENTITY_SHARING,
        ],
      },
      {
        name: 'Window & Screen Management',
        settings: [
          ContentSettingsType.ALL_SCREEN_CAPTURE,
          ContentSettingsType.AUTOMATIC_FULLSCREEN,
          ContentSettingsType.AUTO_PICTURE_IN_PICTURE,
          ContentSettingsType.CAPTURED_SURFACE_CONTROL,
          ContentSettingsType.DISPLAY_CAPTURE,
          ContentSettingsType.DISPLAY_MEDIA_SYSTEM_AUDIO,
          ContentSettingsType.WAKE_LOCK_SCREEN,
          ContentSettingsType.WAKE_LOCK_SYSTEM,
          ContentSettingsType.WINDOW_MANAGEMENT,
        ],
      },
      {
        name: 'Background Services',
        settings: [
          ContentSettingsType.BACKGROUND_FETCH,
          ContentSettingsType.BACKGROUND_SYNC,
          ContentSettingsType.PERIODIC_BACKGROUND_SYNC,
        ],
      },
      {
        name: 'Site Features & Integration',
        settings: [
          ContentSettingsType.APP_BANNER,
          ContentSettingsType.CONTROLLED_FRAME,
          ContentSettingsType.IDLE_DETECTION,
          ContentSettingsType.INITIALIZED_TRANSLATIONS,
          ContentSettingsType.INTENT_PICKER_DISPLAY,
          ContentSettingsType.ON_DEVICE_SPEECH_RECOGNITION_LANGUAGES_DOWNLOADED,
          ContentSettingsType.PAYMENT_HANDLER,
          ContentSettingsType.SUB_APP_INSTALLATION_PROMPTS,
          ContentSettingsType.WEB_APP_INSTALLATION,
          ContentSettingsType.WEB_PRINTING,
        ],
      },
      {
        name: 'Site Behavior & Metadata',
        settings: [
          ContentSettingsType.AUTO_DARK_WEB_CONTENT,
          ContentSettingsType.AUTO_SELECT_CERTIFICATE,
          ContentSettingsType.CLIENT_HINTS,
          ContentSettingsType.FORMFILL_METADATA,
          ContentSettingsType.HTTPS_ENFORCED,
          ContentSettingsType.HTTP_ALLOWED,
          ContentSettingsType.IMPORTANT_SITE_INFO,
          ContentSettingsType.MEDIA_ENGAGEMENT,
          ContentSettingsType.REDUCED_ACCEPT_LANGUAGE,
          ContentSettingsType.REQUEST_DESKTOP_SITE,
          ContentSettingsType.SITE_ENGAGEMENT,
        ],
      },
      {
        // New content settings will automatically appear in this group.
        name: 'Other',
        settings: [
          ContentSettingsType.DEFAULT,
          ContentSettingsType.DEPRECATED_ACCESSIBILITY_EVENTS,
          ContentSettingsType.DEPRECATED_FEDERATED_IDENTITY_ACTIVE_SESSION,
          ContentSettingsType.DEPRECATED_PPAPI_BROKER,
        ],
      },
    ];
