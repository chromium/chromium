// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * All possible contentSettingsTypes that we currently support configuring in
 * the UI. Both top-level categories and content settings that represent
 * individual permissions under Site Details should appear here.
 * This should be kept in sync with the |kContentSettingsTypeGroupNames| array
 * in chrome/browser/ui/webui/site_settings_helper.cc
 * @enum {string}
 */
settings.ContentSettingsTypes = {
  COOKIES: 'cookies',
  IMAGES: 'images',
  JAVASCRIPT: 'javascript',
  SOUND: 'sound',
  PLUGINS: 'plugins',  // AKA Flash.
  POPUPS: 'popups',
  GEOLOCATION: 'location',
  NOTIFICATIONS: 'notifications',
  MIC: 'media-stream-mic',  // AKA Microphone.
  CAMERA: 'media-stream-camera',
  PROTOCOL_HANDLERS: 'register-protocol-handler',
  UNSANDBOXED_PLUGINS: 'ppapi-broker',
  AUTOMATIC_DOWNLOADS: 'multiple-automatic-downloads',
  BACKGROUND_SYNC: 'background-sync',
  MIDI_DEVICES: 'midi-sysex',
  USB_DEVICES: 'usb-devices',
  SERIAL_PORTS: 'serial-ports',
  ZOOM_LEVELS: 'zoom-levels',
  PROTECTED_CONTENT: 'protected-content',
  ADS: 'ads',
  CLIPBOARD: 'clipboard',
  SENSORS: 'sensors',
  PAYMENT_HANDLER: 'payment-handler',
  MIXEDSCRIPT: 'mixed-script',
  BLUETOOTH_SCANNING: 'bluetooth-scanning',
  NATIVE_FILE_SYSTEM_WRITE: 'native-file-system-write',
};

/**
 * Contains the possible string values for a given ContentSettingsTypes.
 * This should be kept in sync with the |ContentSetting| enum in
 * components/content_settings/core/common/content_settings.h
 * @enum {string}
 */
settings.ContentSetting = {
  DEFAULT: 'default',
  ALLOW: 'allow',
  BLOCK: 'block',
  ASK: 'ask',
  SESSION_ONLY: 'session_only',
  IMPORTANT_CONTENT: 'detect_important_content',
};

/**
 * All possible ChooserTypes that we currently support configuring in the UI.
 * This should be kept in sync with the |kChooserTypeGroupNames| array in
 * chrome/browser/ui/webui/site_settings_helper.cc
 * @enum {string}
 */
settings.ChooserType = {
  NONE: '',
  USB_DEVICES: 'usb-devices-data',
  SERIAL_PORTS: 'serial-ports-data',
};

/**
 * Contains the possible sources of a ContentSetting.
 * This should be kept in sync with the |SiteSettingSource| enum in
 * chrome/browser/ui/webui/site_settings_helper.h
 * @enum {string}
 */
settings.SiteSettingSource = {
  ADS_FILTER_BLACKLIST: 'ads-filter-blacklist',
  DEFAULT: 'default',
  // This source is for the Protected Media Identifier / Protected Content
  // content setting only, which is only available on ChromeOS.
  DRM_DISABLED: 'drm-disabled',
  EMBARGO: 'embargo',
  EXTENSION: 'extension',
  INSECURE_ORIGIN: 'insecure-origin',
  KILL_SWITCH: 'kill-switch',
  POLICY: 'policy',
  PREFERENCE: 'preference',
};

/**
 * A category value to use for the All Sites list.
 * @type {string}
 */
settings.ALL_SITES = 'all-sites';

/**
 * An invalid subtype value.
 * @type {string}
 */
settings.INVALID_CATEGORY_SUBTYPE = '';

/**
 * Contains the possible record action types.
 * This should be kept in sync with the |AllSitesAction| enum in
 * chrome/browser/ui/webui/settings/site_settings_handler.cc
 * @enum {number}
 */
settings.AllSitesAction = {
  LOAD_PAGE: 0,
  RESET_PERMISSIONS: 1,
  CLEAR_DATA: 2,
  ENTER_SITE_DETAILS: 3,
};

/**
 * Contains the possible sort methods.
 * @enum {string}
 */
settings.SortMethod = {
  NAME: 'name',
  MOST_VISITED: 'most-visited',
  STORAGE: 'data-stored',
};

/**
 * String representation of the wildcard used for universal
 * match for SiteExceptions.
 * @type {string}
 */
settings.SITE_EXCEPTION_WILDCARD = '*';
