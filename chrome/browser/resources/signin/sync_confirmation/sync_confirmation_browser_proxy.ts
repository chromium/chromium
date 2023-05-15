// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface SyncBenefit {
  title: string;
  iconName: string;
}

/**
 * @fileoverview A helper object used by the sync confirmation dialog to
 * interact with the browser.
 */

export interface SyncConfirmationBrowserProxy {
  /**
   * Called when the user confirms the Sync Confirmation dialog.
   * @param description Strings that the user was presented with in the UI.
   * @param confirmation Text of the element that the user clicked on.
   */
  confirm(description: string[], confirmation: string): void;

  /**
   * Called when the user undoes the Sync confirmation.
   */
  undo(): void;

  /**
   * Called when the user clicks on the Settings link in
   *     the Sync Confirmation dialog.
   * @param description Strings that the user was presented with in the UI.
   * @param confirmation Text of the element that the user clicked on.
   */
  goToSettings(description: string[], confirmation: string): void;

  /**
   * Called when the user clicks on the device settings link.
   */
  openDeviceSyncSettings(): void;

  initializedWithSize(height: number[]): void;

  /**
   * Called when the WebUIListener for "account-info-changed" was added.
   */
  requestAccountInfo(): void;
}

export class SyncConfirmationBrowserProxyImpl implements
    SyncConfirmationBrowserProxy {
  confirm(description: string[], confirmation: string) {
    chrome.send('confirm', [description, confirmation]);
  }

  undo() {
    chrome.send('undo');
  }

  goToSettings(description: string[], confirmation: string) {
    chrome.send('goToSettings', [description, confirmation]);
  }

  initializedWithSize(height: number[]) {
    chrome.send('initializedWithSize', height);
  }

  requestAccountInfo() {
    chrome.send('accountInfoRequest');
  }

  openDeviceSyncSettings() {
    chrome.send('openDeviceSyncSettings');
  }

  static getInstance(): SyncConfirmationBrowserProxy {
    return instance || (instance = new SyncConfirmationBrowserProxyImpl());
  }

  static setInstance(obj: SyncConfirmationBrowserProxy) {
    instance = obj;
  }
}

let instance: SyncConfirmationBrowserProxy|null = null;
