// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PrefsBrowserProxy is an abstraction over
 * chrome.settingsPrivate which facilitates testing.
 */

export type PrefsChangedListener =
    (entries: chrome.settingsPrivate.PrefObject[]) => void;

export interface PrefsBrowserProxy {
  /**
   * Add an observer to the list of prefs.
   */
  addPrefsChangedListener(listener: PrefsChangedListener): void;

  /**
   * Remove an observer from the list of prefs.
   */
  removePrefsChangedListener(listener: PrefsChangedListener): void;

  /**
   * Request the saved pref by key.
   */
  getPref(key: string): Promise<chrome.settingsPrivate.PrefObject>;

  /**
   * Sets the pref value by key.
   */
  setPref(key: string, value: any): Promise<boolean>;
}

export class PrefsBrowserProxyImpl implements PrefsBrowserProxy {
  addPrefsChangedListener(listener: PrefsChangedListener): void {
    chrome.settingsPrivate.onPrefsChanged.addListener(listener);
  }

  removePrefsChangedListener(listener: PrefsChangedListener): void {
    chrome.settingsPrivate.onPrefsChanged.removeListener(listener);
  }

  getPref(key: string): Promise<chrome.settingsPrivate.PrefObject> {
    return chrome.settingsPrivate.getPref(key);
  }

  setPref(key: string, value: any): Promise<boolean> {
    return chrome.settingsPrivate.setPref(key, value, /* pageId */ '');
  }

  static getInstance(): PrefsBrowserProxy {
    return instance || (instance = new PrefsBrowserProxyImpl());
  }

  static setInstance(obj: PrefsBrowserProxy) {
    instance = obj;
  }
}

let instance: PrefsBrowserProxy|null = null;
