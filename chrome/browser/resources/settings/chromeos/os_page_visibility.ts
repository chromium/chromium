// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * Specifies page visibility based on incognito status and ChromeOS guest mode.
 */
export interface OsPageVisibility {
  a11y: boolean;
  advancedSettings: boolean;
  appearance: {
    bookmarksBar: boolean,
    homeButton: boolean,
    pageZoom: boolean,
    setTheme: boolean,
    setWallpaper: boolean,
  };
  autofill: boolean;
  bluetooth: boolean;
  dateTime: boolean;
  device: boolean;
  downloads: {
    googleDrive: boolean,
    smbShares: boolean,
  };
  extensions: boolean;
  internet: boolean;
  kerberos: boolean;
  languages: {
    manageInputMethods: boolean,
    inputMethodsList: boolean,
  };
  multidevice: boolean;
  onStartup: boolean;
  people: boolean|{
    googleAccounts: boolean,
    lockScreen: boolean,
    manageUsers: boolean,
  };
  printing: boolean;
  privacy: {
    contentProtectionAttestation: boolean,
    networkPrediction: boolean,
    searchPrediction: boolean,
    wakeOnWifi: boolean,
  };
  reset: boolean;
}

const isAccountManagerEnabled =
    loadTimeData.valueExists('isAccountManagerEnabled') &&
    loadTimeData.getBoolean('isAccountManagerEnabled');
const isKerberosEnabled = loadTimeData.valueExists('isKerberosEnabled') &&
    loadTimeData.getBoolean('isKerberosEnabled');
const isGuestMode = loadTimeData.getBoolean('isGuest');

/**
 * Dictionary defining page visibility.
 */
let osPageVisibility: OsPageVisibility;
if (isGuestMode) {
  osPageVisibility = {
    a11y: true,
    advancedSettings: true,
    appearance: {
      setWallpaper: false,
      setTheme: false,
      homeButton: false,
      bookmarksBar: false,
      pageZoom: false,
    },
    autofill: false,
    bluetooth: true,
    dateTime: true,
    device: true,
    downloads: {
      googleDrive: false,
      smbShares: false,
    },
    extensions: false,
    internet: true,
    kerberos: isKerberosEnabled,
    languages: {
      manageInputMethods: true,
      inputMethodsList: true,
    },
    multidevice: false,
    onStartup: false,
    people: false,
    printing: true,
    privacy: {
      contentProtectionAttestation: true,
      searchPrediction: false,
      networkPrediction: false,
      wakeOnWifi: true,
    },
    reset: false,
  };
} else {
  osPageVisibility = {
    a11y: true,
    advancedSettings: true,
    appearance: {
      setWallpaper: true,
      setTheme: true,
      homeButton: true,
      bookmarksBar: true,
      pageZoom: true,
    },
    autofill: true,
    bluetooth: true,
    dateTime: true,
    device: true,
    downloads: {
      googleDrive: true,
      smbShares: true,
    },
    extensions: true,
    internet: true,
    kerberos: isKerberosEnabled,
    languages: {
      manageInputMethods: true,
      inputMethodsList: true,
    },
    multidevice: true,
    onStartup: true,
    people: {
      lockScreen: true,
      googleAccounts: isAccountManagerEnabled,
      manageUsers: true,
    },
    printing: true,
    privacy: {
      contentProtectionAttestation: true,
      searchPrediction: true,
      networkPrediction: true,
      wakeOnWifi: true,
    },
    reset: true,
  };
}

export {osPageVisibility};
