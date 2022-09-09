// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * User preferences for OS sync. 'Registered' means the user has the option to
 * select a type. For example, a type might not be registered due to a feature
 * flag being disabled.
 * @see components/sync/driver/sync_service.h
 *
 * TODO(jamescook): Encryption options.
 *
 * @typedef {{
 *   osAppsRegistered: boolean,
 *   osAppsSynced: boolean,
 *   osPreferencesRegistered: boolean,
 *   osPreferencesSynced: boolean,
 *   syncAllOsDataTypes: boolean,
 *   wallpaperEnabled: boolean,
 *   osWifiConfigurationsRegistered: boolean,
 *   osWifiConfigurationsSynced: boolean,
 * }}
 */
export let OsSyncPrefs;

/** @interface */
export class OsSyncBrowserProxy {
  /**
   * Function to invoke when the sync page has been navigated to. This
   * registers the UI as the "active" sync UI.
   */
  didNavigateToOsSyncPage() {}

  /**
   * Function to invoke when leaving the sync page so that the C++ layer can
   * be notified that the sync UI is no longer open.
   */
  didNavigateAwayFromOsSyncPage() {}

  /**
   * Function to invoke when the WebUI wants an update of the OsSyncPrefs.
   */
  sendOsSyncPrefsChanged() {}

  /**
   * Sets which types of data to sync.
   * @param {!OsSyncPrefs} osSyncPrefs
   */
  setOsSyncDatatypes(osSyncPrefs) {}
}

/** @type {?OsSyncBrowserProxy} */
let instance = null;

/**
 * @implements {OsSyncBrowserProxy}
 */
export class OsSyncBrowserProxyImpl {
  /** @return {!OsSyncBrowserProxy} */
  static getInstance() {
    return instance || (instance = new OsSyncBrowserProxyImpl());
  }

  /** @param {!OsSyncBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  didNavigateToOsSyncPage() {
    chrome.send('DidNavigateToOsSyncPage');
  }

  /** @override */
  didNavigateAwayFromOsSyncPage() {
    chrome.send('DidNavigateAwayFromOsSyncPage');
  }

  /** @override */
  sendOsSyncPrefsChanged() {
    chrome.send('OsSyncPrefsDispatch');
  }

  /** @override */
  setOsSyncDatatypes(osSyncPrefs) {
    return chrome.send('SetOsSyncDatatypes', [osSyncPrefs]);
  }
}
