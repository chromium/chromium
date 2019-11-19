// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Specifies page visibility based on incognito status, Chrome OS guest mode,
 * and whether or not to include OS settings. Once the Chrome OS SplitSettings
 * project is completed this can be changed to only consider incognito and
 * guest mode. https://crbug.com/950007
 * @typedef {{
 *   a11y: (boolean|undefined|A11yPageVisibility),
 *   advancedSettings: (boolean|undefined),
 *   appearance: (boolean|undefined|AppearancePageVisibility),
 *   autofill: (boolean|undefined),
 *   bluetooth: (boolean|undefined),
 *   dateTime: (boolean|undefined),
 *   defaultBrowser: (boolean|undefined),
 *   device: (boolean|undefined),
 *   downloads: (boolean|undefined|DownloadsPageVisibility),
 *   internet: (boolean|undefined),
 *   languages: (boolean|undefined|LanguagesPageVisibility),
 *   multidevice: (boolean|undefined),
 *   onStartup: (boolean|undefined),
 *   people: (boolean|undefined|PeoplePageVisibility),
 *   printing: (boolean|undefined),
 *   privacy: (boolean|undefined|PrivacyPageVisibility),
 *   reset:(boolean|undefined|ResetPageVisibility),
 * }}
 */
let PageVisibility;

/**
 * @typedef {{
 *   webstoreLink: boolean,
 * }}
 */
let A11yPageVisibility;

/**
 * TODO(crbug.com/950007): Remove setWallpaper after SplitSettings launch.
 * @typedef {{
 *   bookmarksBar: boolean,
 *   homeButton: boolean,
 *   pageZoom: boolean,
 *   setTheme: boolean,
 *   setWallpaper: boolean,
 * }}
 */
let AppearancePageVisibility;

/**
 * @typedef {{
 *   googleDrive: boolean,
 *   smbShares: boolean,
 * }}
 */
let DownloadsPageVisibility;

/**
 * @typedef {{
 *   googleAccounts: boolean,
 *   kerberosAccounts: boolean,
 *   lockScreen: boolean,
 *   manageUsers: boolean,
 * }}
 */
let PeoplePageVisibility;

/**
 * @typedef {{
 *   contentProtectionAttestation: boolean,
 *   networkPrediction: boolean,
 *   searchPrediction: boolean,
 *   wakeOnWifi: boolean,
 * }}
 */
let PrivacyPageVisibility;

/**
 * @typedef {{
 *   powerwash: boolean,
 * }}
 */
let ResetPageVisibility;

/**
 * @typedef {{
 *   manageInputMethods: boolean,
 *   inputMethodsList: boolean,
 * }}
 */
let LanguagesPageVisibility;

cr.define('settings', function() {
  /**
   * Dictionary defining page visibility.
   * @type {!PageVisibility}
   */
  let pageVisibility;

  const showOSSettings = loadTimeData.getBoolean('showOSSettings');
  const isAccountManagerEnabled =
      loadTimeData.valueExists('isAccountManagerEnabled') &&
      loadTimeData.getBoolean('isAccountManagerEnabled');
  const isKerberosEnabled = loadTimeData.valueExists('isKerberosEnabled') &&
      loadTimeData.getBoolean('isKerberosEnabled');

  if (loadTimeData.getBoolean('isGuest')) {
    // "if not chromeos" and "if chromeos" in two completely separate blocks
    // to work around closure compiler.
    // <if expr="not chromeos">
    pageVisibility = {
      autofill: false,
      people: false,
      onStartup: false,
      reset: false,
      appearance: false,
      defaultBrowser: false,
      advancedSettings: false,
      extensions: false,
      printing: false,
      languages: false,
    };
    // </if>
    // <if expr="chromeos">
    pageVisibility = {
      internet: showOSSettings,
      bluetooth: showOSSettings,
      multidevice: false,
      autofill: false,
      people: false,
      onStartup: false,
      reset: false,
      appearance: {
        setWallpaper: false,
        setTheme: false,
        homeButton: false,
        bookmarksBar: false,
        pageZoom: false,
      },
      device: showOSSettings,
      advancedSettings: true,
      dateTime: showOSSettings,
      privacy: {
        contentProtectionAttestation: showOSSettings,
        searchPrediction: false,
        networkPrediction: false,
        wakeOnWifi: showOSSettings,
      },
      downloads: {
        googleDrive: false,
        smbShares: false,
      },
      a11y: {
        webstoreLink: showOSSettings,
      },
      extensions: false,
      printing: true,
      languages: {
        manageInputMethods: showOSSettings,
        inputMethodsList: showOSSettings,
      },
    };
    // </if>
  } else {
    // All pages are visible when not in chromeos. Since polymer only notifies
    // after a property is set.
    // <if expr="chromeos">
    pageVisibility = {
      internet: showOSSettings,
      bluetooth: showOSSettings,
      multidevice: showOSSettings,
      autofill: true,
      people: {
        lockScreen: showOSSettings,
        kerberosAccounts: showOSSettings && isKerberosEnabled,
        googleAccounts: showOSSettings && isAccountManagerEnabled,
        manageUsers: showOSSettings,
      },
      onStartup: true,
      reset: {
        powerwash: showOSSettings,
      },
      appearance: {
        setWallpaper: showOSSettings,
        setTheme: true,
        homeButton: true,
        bookmarksBar: true,
        pageZoom: true,
      },
      device: showOSSettings,
      advancedSettings: true,
      dateTime: showOSSettings,
      privacy: {
        contentProtectionAttestation: showOSSettings,
        searchPrediction: true,
        networkPrediction: true,
        wakeOnWifi: showOSSettings,
      },
      downloads: {
        googleDrive: showOSSettings,
        smbShares: showOSSettings,
      },
      a11y: {
        webstoreLink: showOSSettings,
      },
      extensions: true,
      printing: true,
      languages: {
        manageInputMethods: showOSSettings,
        inputMethodsList: showOSSettings,
      },
    };
    // </if>
  }

  return {pageVisibility: pageVisibility};
});
