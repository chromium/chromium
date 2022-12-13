// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

/**
 * Specifies page visibility based on incognito status and Chrome OS guest mode.
 * @typedef {{
 *   a11y: (boolean|undefined),
 *   advancedSettings: (boolean|undefined),
 *   appearance: (OSAppearancePageVisibility|undefined),
 *   autofill: (boolean|undefined),
 *   bluetooth: (boolean|undefined),
 *   dateTime: (boolean|undefined),
 *   device: (boolean|undefined),
 *   downloads: (DownloadsPageVisibility|undefined),
 *   internet: (boolean|undefined),
 *   kerberos: (boolean|undefined),
 *   languages: (LanguagesPageVisibility|undefined),
 *   multidevice: (boolean|undefined),
 *   onStartup: (boolean|undefined),
 *   people: (boolean|undefined|PeoplePageVisibility),
 *   printing: (boolean|undefined),
 *   privacy: (OSPrivacyPageVisibility|undefined),
 *   reset: (boolean|undefined),
 * }}
 */
export let OSPageVisibility;

/**
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 *   setWallpaper: boolean,
 * }}
 */
export let OSAppearancePageVisibility;

/**
 * @typedef {{
 *   googleDrive: boolean,
 *   smbShares: boolean,
 * }}
 */
export let DownloadsPageVisibility;

/**
 * @typedef {{
 *   googleAccounts: boolean,
 *   lockScreen: boolean,
 *   manageUsers: boolean,
 * }}
 */
export let PeoplePageVisibility;

/**
 * @typedef {{
 *   contentProtectionAttestation: boolean,
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 *   wakeOnWifi: boolean,
 * }}
 */
export let OSPrivacyPageVisibility;

/**
 * @typedef {{
 *   manageInputMethods: boolean,
 *   inputMethodsList: boolean,
 * }}
 */
export let LanguagesPageVisibility;

/**
 * Dictionary defining page visibility.
 * @type {!OSPageVisibility}
 */
export let osPageVisibility;

const isAccountManagerEnabled =
    loadTimeData.valueExists('isAccountManagerEnabled') &&
    loadTimeData.getBoolean('isAccountManagerEnabled');
const isKerberosEnabled = loadTimeData.valueExists('isKerberosEnabled') &&
    loadTimeData.getBoolean('isKerberosEnabled');

if (loadTimeData.getBoolean('isGuest')) {
  osPageVisibility = {
    internet: true,
    bluetooth: true,
    multidevice: false,
    autofill: false,
    people: false,
    kerberos: isKerberosEnabled,
    onStartup: false,
    reset: false,
    appearance: {
      setWallpaper: false,
      setTheme: false,
      homeButton: false,
      bookmarksBar: false,
      pageZoom: false,
    },
    device: true,
    advancedSettings: true,
    dateTime: true,
    privacy: {
      contentProtectionAttestation: true,
      searchPrediction: false,
      networkPrediction: false,
      wakeOnWifi: true,
    },
    downloads: {
      googleDrive: false,
      smbShares: false,
    },
    a11y: true,
    extensions: false,
    printing: true,
    languages: {
      manageInputMethods: true,
      inputMethodsList: true,
    },
  };
} else {
  osPageVisibility = {
    internet: true,
    bluetooth: true,
    multidevice: true,
    autofill: true,
    people: {
      lockScreen: true,
      googleAccounts: isAccountManagerEnabled,
      manageUsers: true,
    },
    kerberos: isKerberosEnabled,
    onStartup: true,
    reset: true,
    appearance: {
      setWallpaper: true,
      setTheme: true,
      homeButton: true,
      bookmarksBar: true,
      pageZoom: true,
    },
    device: true,
    advancedSettings: true,
    dateTime: true,
    privacy: {
      contentProtectionAttestation: true,
      searchPrediction: true,
      networkPrediction: true,
      wakeOnWifi: true,
    },
    downloads: {
      googleDrive: true,
      smbShares: true,
    },
    a11y: true,
    extensions: true,
    printing: true,
    languages: {
      manageInputMethods: true,
      inputMethodsList: true,
    },
  };
}
