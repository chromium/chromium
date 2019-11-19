// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the sync confirmation dialog to
 * interact with the browser.
 */
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class SyncConfirmationBrowserProxy {
  /**
   * Called when the user confirms the Sync Confirmation dialog.
   * @param {!Array<string>} description Strings that the user was presented
   *     with in the UI.
   * @param {string} confirmation Text of the element that the user
   *     clicked on.
   */
  confirm(description, confirmation) {}

  /**
   * Called when the user undoes the Sync confirmation.
   */
  undo() {}

  /**
   * Called when the user clicks on the Settings link in
   *     the Sync Confirmation dialog.
   * @param {!Array<string>} description Strings that the user was presented
   *     with in the UI.
   * @param {string} confirmation Text of the element that the user
   *     clicked on.
   */
  goToSettings(description, confirmation) {}

  /** @param {!Array<number>} height */
  initializedWithSize(height) {}

  /**
   * Called when the WebUIListener for "account-image-changed" was added.
   */
  requestAccountImage() {}
}

/** @implements {SyncConfirmationBrowserProxy} */
export class SyncConfirmationBrowserProxyImpl {
  /** @override */
  confirm(description, confirmation) {
    chrome.send('confirm', [description, confirmation]);
  }

  /** @override */
  undo() {
    chrome.send('undo');
  }

  /** @override */
  goToSettings(description, confirmation) {
    chrome.send('goToSettings', [description, confirmation]);
  }

  /** @override */
  initializedWithSize(height) {
    chrome.send('initializedWithSize', height);
  }

  /** @override */
  requestAccountImage() {
    chrome.send('accountImageRequest');
  }
}

addSingletonGetter(SyncConfirmationBrowserProxyImpl);
