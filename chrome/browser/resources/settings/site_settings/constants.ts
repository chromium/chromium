// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * All possible contentSettingsTypes that we currently support configuring in
 * the UI. Both top-level categories and content settings that represent
 * individual permissions under Site Details should appear here.
 * This should be kept in sync with the |kContentSettingsTypeGroupNames| array
 * in chrome/browser/ui/webui/site_settings_helper.cc
 */
export enum ContentSettingsTypes {
  ADS = 'ads',
  AR = 'ar',
  AUTOMATIC_DOWNLOADS = 'multiple-automatic-downloads',
  BACKGROUND_SYNC = 'background-sync',
  BLUETOOTH_DEVICES = 'bluetooth-devices',
  BLUETOOTH_SCANNING = 'bluetooth-scanning',
  CAMERA = 'media-stream-camera',
  CLIPBOARD = 'clipboard',
  COOKIES = 'cookies',
  FILE_SYSTEM_WRITE = 'file-system-write',
  FONT_ACCESS = 'font-access',
  GEOLOCATION = 'location',
  HID_DEVICES = 'hid-devices',
  IDLE_DETECTION = 'idle-detection',
  IMAGES = 'images',
  JAVASCRIPT = 'javascript',
  MIC = 'media-stream-mic',  // AKA Microphone.
  MIDI_DEVICES = 'midi-sysex',
  MIXEDSCRIPT = 'mixed-script',
  NOTIFICATIONS = 'notifications',
  PAYMENT_HANDLER = 'payment-handler',
  POPUPS = 'popups',
  PROTECTED_CONTENT = 'protected-content',
  PROTOCOL_HANDLERS = 'register-protocol-handler',
  SENSORS = 'sensors',
  SERIAL_PORTS = 'serial-ports',
  SOUND = 'sound',
  USB_DEVICES = 'usb-devices',
  VR = 'vr',
  WINDOW_PLACEMENT = 'window-placement',
  ZOOM_LEVELS = 'zoom-levels',

  // The following item is not in the C++ kContentSettingsTypeGroupNames, but it
  // is used everywhere where ContentSettingsTypes is used in JS.
  PDF_DOCUMENTS = 'pdfDocuments',
}

/**
 * Contains the possible string values for a given ContentSettingsTypes.
 * This should be kept in sync with the |ContentSetting| enum in
 * components/content_settings/core/common/content_settings.h
 */
export enum ContentSetting {
  DEFAULT = 'default',
  ALLOW = 'allow',
  BLOCK = 'block',
  ASK = 'ask',
  SESSION_ONLY = 'session_only',
  IMPORTANT_CONTENT = 'detect_important_content',
}

/**
 * All possible ChooserTypes that we currently support configuring in the UI.
 * This should be kept in sync with the |kChooserTypeGroupNames| array in
 * chrome/browser/ui/webui/site_settings_helper.cc
 */
export enum ChooserType {
  NONE = '',
  USB_DEVICES = 'usb-devices-data',
  SERIAL_PORTS = 'serial-ports-data',
  HID_DEVICES = 'hid-devices-data',
  BLUETOOTH_DEVICES = 'bluetooth-devices-data',
}

/**
 * Possible preference settings for the profile.cookie_controls_mode pref.
 * This should be kept in sync with the |CookieControlsMode| enum in
 * components/content_settings/core/browser/cookie_settings.h
 */
export enum CookieControlsMode {
  OFF = 0,
  BLOCK_THIRD_PARTY = 1,
  INCOGNITO_ONLY = 2,
}

/**
 * Contains the possible sources of a ContentSetting.
 * This should be kept in sync with the |SiteSettingSource| enum in
 * chrome/browser/ui/webui/site_settings_helper.h
 */
export enum SiteSettingSource {
  ADS_FILTER_BLACKLIST = 'ads-filter-blacklist',
  ALLOWLIST = 'allowlist',
  DEFAULT = 'default',
  EMBARGO = 'embargo',
  EXTENSION = 'extension',
  HOSTED_APP = 'HostedApp',
  INSECURE_ORIGIN = 'insecure-origin',
  KILL_SWITCH = 'kill-switch',
  POLICY = 'policy',
  PREFERENCE = 'preference',
}

/**
 * Enumeration of states for the notification default setting generated pref.
 * Must be kept in sync with the enum of the same name located in:
 * chrome/browser/content_settings/generated_notification_pref.h
 */
export enum NotificationSetting {
  ASK = 0,
  QUIETER_MESSAGING = 1,
  BLOCK = 2,
}

/**
 * An invalid subtype value.
 */
export const INVALID_CATEGORY_SUBTYPE: string = '';

/**
 * Contains the possible record action types.
 * This should be kept in sync with the |AllSitesAction2| enum in
 * chrome/browser/ui/webui/settings/site_settings_handler.cc
 */
export enum AllSitesAction2 {
  LOAD_PAGE = 0,
  RESET_SITE_GROUP_PERMISSIONS = 1,
  RESET_ORIGIN_PERMISSIONS = 2,
  CLEAR_ALL_DATA = 3,
  CLEAR_SITE_GROUP_DATA = 4,
  CLEAR_ORIGIN_DATA = 5,
  ENTER_SITE_DETAILS = 6,
  REMOVE_SITE_GROUP = 7,
  REMOVE_ORIGIN = 8,
  REMOVE_ORIGIN_PARTITIONED = 9,
}

/**
 * Contains the possible sort methods.
 */
export enum SortMethod {
  NAME = 'name',
  MOST_VISITED = 'most-visited',
  STORAGE = 'data-stored',
}

/**
 * Contains types of dialogs on the AllSites page,
 * used for logging userActions.
 */
export enum ALL_SITES_DIALOG {
  CLEAR_DATA = 'ClearData',
  RESET_PERMISSIONS = 'ResetPermissions',
}

/**
 * String representation of the wildcard used for universal
 * match for SiteExceptions.
 */
export const SITE_EXCEPTION_WILDCARD: string = '*';
