// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * User preferences for OS sync. 'Registered' means the user has the option to
 * select a type. For example, a type might not be registered due to a feature
 * flag being disabled.
 * @see components/sync/service/sync_service.h
 *
 * TODO(jamescook): Encryption options.
 */
export interface OsSyncPrefs {
  osAppsRegistered: boolean;
  osAppsSynced: boolean;
  osPreferencesRegistered: boolean;
  osPreferencesSynced: boolean;
  syncAllOsTypes: boolean;
  wallpaperEnabled: boolean;
  osWifiConfigurationsRegistered: boolean;
  osWifiConfigurationsSynced: boolean;
}

export interface OsSyncBrowserProxy {
  /**
   * Function to invoke when the sync page has been navigated to. This
   * registers the UI as the "active" sync UI.
   */
  didNavigateToOsSyncPage(): void;

  /**
   * Function to invoke when leaving the sync page so that the C++ layer can
   * be notified that the sync UI is no longer open.
   */
  didNavigateAwayFromOsSyncPage(): void;

  /**
   * Function to invoke when the WebUI wants an update of the OsSyncPrefs.
   */
  sendOsSyncPrefsChanged(): void;

  /**
   * Sets which types of data to sync.
   */
  setOsSyncDatatypes(osSyncPrefs: OsSyncPrefs): void;
}

let instance: OsSyncBrowserProxy|null = null;

export class OsSyncBrowserProxyImpl implements OsSyncBrowserProxy {
  static getInstance(): OsSyncBrowserProxy {
    return instance || (instance = new OsSyncBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsSyncBrowserProxy): void {
    instance = obj;
  }

  didNavigateToOsSyncPage(): void {
    chrome.send('DidNavigateToOsSyncPage');
  }

  didNavigateAwayFromOsSyncPage(): void {
    chrome.send('DidNavigateAwayFromOsSyncPage');
  }

  sendOsSyncPrefsChanged(): void {
    chrome.send('OsSyncPrefsDispatch');
  }

  setOsSyncDatatypes(osSyncPrefs: OsSyncPrefs): void {
    chrome.send('SetOsSyncDatatypes', [osSyncPrefs]);
  }
}