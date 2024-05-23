// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface SyncBenefit {
  title: string;
  iconName: string;
}

// LINT.IfChange(screen_mode)
/**
 * In PENDING mode, the screen should not show consent buttons and indicate that
 * some loading is pending. In RESTRICTED mode, the button must not be weighted,
 * and in UNRESTRICTED mode they can be.
 *
 * In UNSUPPORTED mode, the client take any behavior.
 */
export enum ScreenMode {
  UNSUPPORTED = 0,
  PENDING = 1,
  RESTRICTED = 2,
  UNRESTRICTED = 3,
  DEADLINED = 4,
}
// LINT.ThenChange(//chrome/browser/ui/webui/signin/sync_confirmation_handler.h:screen_mode)

/**
 * @fileoverview A helper object used by the sync confirmation dialog to
 * interact with the browser.
 */

export interface SyncConfirmationBrowserProxy {
  /**
   * Called when the user confirms the Sync Confirmation dialog.
   * @param description Strings that the user was presented with in the UI.
   * @param confirmation Text of the element that the user clicked on.
   * @param screenMode serialized identifier of the screen mode.
   */
  confirm(description: string[], confirmation: string, screenMode: ScreenMode):
      void;

  /**
   * Called when the user undoes the Sync confirmation.
   * @param screenMode serialized identifier of the screen mode.
   */
  undo(screenMode: ScreenMode): void;

  /**
   * Called when the user clicks on the Settings link in
   *     the Sync Confirmation dialog.
   * @param description Strings that the user was presented with in the UI.
   * @param confirmation Text of the element that the user clicked on.
   * @param screenMode serialized identifier of the screen mode.
   */
  goToSettings(
      description: string[], confirmation: string,
      screenMode: ScreenMode): void;

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
  confirm(description: string[], confirmation: string, screenMode: ScreenMode) {
    chrome.send('confirm', [description, confirmation, screenMode]);
  }

  undo(screenMode: ScreenMode) {
    chrome.send('undo', [screenMode]);
  }

  goToSettings(
      description: string[], confirmation: string, screenMode: ScreenMode) {
    chrome.send('goToSettings', [description, confirmation, screenMode]);
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
