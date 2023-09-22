// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Browser Proxy for Parental Controls functions.
 */

export interface ParentalControlsBrowserProxy {
  /**
   * Shows the Add Supervsion dialog.
   */
  showAddSupervisionDialog(): void;

  /**
   * Launches an app that shows the Family Link Settings.  Depending
   * on whether the Family Link Helper app is available, this might
   * launch the app, or take some kind of backup/default action.
   */
  launchFamilyLinkSettings(): void;
}

let instance: ParentalControlsBrowserProxy|null = null;

export class ParentalControlsBrowserProxyImpl implements
    ParentalControlsBrowserProxy {
  static getInstance(): ParentalControlsBrowserProxy {
    return instance || (instance = new ParentalControlsBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: ParentalControlsBrowserProxy): void {
    instance = obj;
  }

  showAddSupervisionDialog(): void {
    chrome.send('showAddSupervisionDialog');
  }

  launchFamilyLinkSettings(): void {
    chrome.send('launchFamilyLinkSettings');
  }
}
