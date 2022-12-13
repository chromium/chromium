// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * All possible contentSettingsTypes that we currently support configuring in
 * the UI. Both top-level categories and content settings that represent
 * individual permissions under Site Details should appear here.
 * This should be kept in sync with the |kContentSettingsTypeGroupNames| array
 * in chrome/browser/ui/webui/settings/site_settings_helper.cc. See
 * chrome/browser/resources/settings/site_settings_page/site_settings_page_util
 * for translations.
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
  FEDERATED_IDENTITY_API = 'federated-identity-api',
  FILE_SYSTEM_WRITE = 'file-system-write',
  GEOLOCATION = 'location',
  HID_DEVICES = 'hid-devices',
  IDLE_DETECTION = 'idle-detection',
  IMAGES = 'images',
  JAVASCRIPT = 'javascript',
  LOCAL_FONTS = 'local-fonts',
  MIC = 'media-stream-mic',  // AKA Microphone.
  MIDI_DEVICES = 'midi-sysex',
  MIXEDSCRIPT = 'mixed-script',
  NOTIFICATIONS = 'notifications',
  PAYMENT_HANDLER = 'payment-handler',
  POPUPS = 'popups',
  PRIVATE_NETWORK_DEVICES = 'private-network-devices',
  PROTECTED_CONTENT = 'protected-content',
  PROTOCOL_HANDLERS = 'register-protocol-handler',
  SENSORS = 'sensors',
  SERIAL_PORTS = 'serial-ports',
  SOUND = 'sound',
  USB_DEVICES = 'usb-devices',
  VR = 'vr',
  WINDOW_MANAGEMENT = 'window-placement',
  ZOOM_LEVELS = 'zoom-levels',

  // The following items are not in the C++ kContentSettingsTypeGroupNames, but
  // are used everywhere where ContentSettingsTypes is used in JS.
  PDF_DOCUMENTS = 'pdfDocuments',
  SITE_DATA = 'site-data',
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
 * chrome/browser/ui/webui/settings/site_settings_helper.cc
 */
export enum ChooserType {
  NONE = '',
  USB_DEVICES = 'usb-devices-data',
  SERIAL_PORTS = 'serial-ports-data',
  HID_DEVICES = 'hid-devices-data',
  BLUETOOTH_DEVICES = 'bluetooth-devices-data',
  PRIVATE_NETWORK_DEVICES = 'private-network-devices-data',
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
 * chrome/browser/ui/webui/settings/site_settings_helper.h
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
  FILTER_BY_FPS_OWNER = 10,
  DELETE_FOR_ENTIRE_FPS = 11,
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
export enum AllSitesDialog {
  CLEAR_DATA = 'ClearData',
  RESET_PERMISSIONS = 'ResetPermissions',
}

/**
 * String representation of the wildcard used for universal
 * match for SiteExceptions.
 */
export const SITE_EXCEPTION_WILDCARD: string = '*';

/**
 * Corresponds to the animation-duration CSS parameter defined in
 * chrome/browser/resources/settings/site_settings_page/site_review_shared.css.
 * Set to be slightly higher, as we want to ensure that the animation is
 * finished before updating the model for the right visual effect.
 */
export const MODEL_UPDATE_DELAY_MS = 300;

/**
 * Types of cookies exceptions based on the use of wildcard in the patterns:
 * - THIRD_PARTY: primary pattern is wildcard (third-party exception).
 * - SITE_DATA: primary pattern is set, secondary pattern is wildcard (site data
 * exceptions) or is set (only possible via extensions API).
 * - COMBINED: any pattern combination can be used.
 */
export enum CookiesExceptionType {
  THIRD_PARTY = 'third-party',
  SITE_DATA = 'site-data',
  COMBINED = 'combined',
}
