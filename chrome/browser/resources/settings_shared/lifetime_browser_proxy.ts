// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="not is_chromeos">
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// </if>

export interface LifetimeBrowserProxy {
  // Triggers a browser restart.
  restart(): void;

  // Triggers a browser relaunch.
  relaunch(): void;

  // <if expr="not is_chromeos">
  // Indicates whether a relaunch confirmation dialog needs to be shown or not.
  shouldShowRelaunchConfirmationDialog(alwaysShowDialog: boolean):
      Promise<boolean>;

  // Returns the description of the relaunch confirmation dialog.
  // A null value can be returned if the condition to show the relaunch dialog
  // is no longer true.
  getRelaunchConfirmationDialogDescription(isVersionUpdate: boolean):
      Promise<string|null>;
  // </if>

  // <if expr="is_chromeos">
  // First signs out current user and then performs a restart.
  signOutAndRestart(): void;

  /**
   * Triggers a factory reset. The parameter indicates whether to install a
   * TPM firmware update (if available) after the reset.
   */
  factoryReset(requestTpmFirmwareUpdate: boolean): void;
  // </if>
}

export class LifetimeBrowserProxyImpl implements LifetimeBrowserProxy {
  restart() {
    chrome.send('restart');
  }

  relaunch() {
    chrome.send('relaunch');
  }

  // <if expr="not is_chromeos">
  shouldShowRelaunchConfirmationDialog(alwaysShowDialog: boolean) {
    return sendWithPromise(
        'shouldShowRelaunchConfirmationDialog', alwaysShowDialog);
  }

  getRelaunchConfirmationDialogDescription(isVersionUpdate: boolean) {
    return sendWithPromise(
        'getRelaunchConfirmationDialogDescription', isVersionUpdate);
  }
  // </if>

  // <if expr="is_chromeos">
  signOutAndRestart() {
    chrome.send('signOutAndRestart');
  }

  factoryReset(requestTpmFirmwareUpdate: boolean) {
    chrome.send('factoryReset', [requestTpmFirmwareUpdate]);
  }
  // </if>

  static getInstance(): LifetimeBrowserProxy {
    return instance || (instance = new LifetimeBrowserProxyImpl());
  }

  static setInstance(obj: LifetimeBrowserProxy) {
    instance = obj;
  }
}

let instance: LifetimeBrowserProxy|null = null;
