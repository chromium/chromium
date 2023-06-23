// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the os settings ui to
 * interact with the browser and send Settings HaTS notifications.
 */

export interface OsSettingsHatsBrowserProxy {
  /**
   * Sends trigger for Os Settings HaTS.
   */
  sendSettingsHats(): void;

  /**
   * Whether the user has navigated and typed into the
   * search bar in Settings once per each Settings session.
   * This method gets called after the user has entered a single letter, and
   * does not depend on if the user utilizes the search results or not.
   */
  settingsUsedSearch(): void;
}

let instance: OsSettingsHatsBrowserProxy|null = null;

export class OsSettingsHatsBrowserProxyImpl implements
    OsSettingsHatsBrowserProxy {
  static getInstance(): OsSettingsHatsBrowserProxy {
    return instance || (instance = new OsSettingsHatsBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: OsSettingsHatsBrowserProxy): void {
    instance = obj;
  }

  sendSettingsHats(): void {
    chrome.send('sendSettingsHats');
  }

  settingsUsedSearch(): void {
    chrome.send('settingsUsedSearch');
  }
}
