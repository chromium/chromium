// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
export class ChromeCleanupProxy {
  /**
   * Registers the current ChromeCleanupHandler as an observer of
   * ChromeCleanerController events.
   */
  registerChromeCleanerObserver() {}

  /**
   * Starts scanning the user's computer.
   * @param {boolean} logsUploadEnabled
   * @param {boolean} notificationEnabled
   */
  startScanning(logsUploadEnabled, notificationEnabled) {}

  /**
   * Starts a cleanup on the user's computer.
   * @param {boolean} logsUploadEnabled
   */
  startCleanup(logsUploadEnabled) {}

  /**
   * Restarts the user's computer.
   */
  restartComputer() {}

  /**
   * Notifies Chrome that the state of the details section changed.
   * @param {boolean} enabled
   */
  notifyShowDetails(enabled) {}

  /**
   * Notifies Chrome that the "learn more" link was clicked.
   */
  notifyLearnMoreClicked() {}

  /**
   * Requests the plural string for the "show more" link in the detailed
   * view for either files to delete or registry keys.
   * @param {number} numHiddenItems
   * @return {!Promise<string>}
   */
  getMoreItemsPluralString(numHiddenItems) {}

  /**
   * Requests the plural string for the "items to remove" link in the detailed
   * view.
   * @param {number} numItems
   * @return {!Promise<string>}
   */
  getItemsToRemovePluralString(numItems) {}
}

/**
 * @implements {ChromeCleanupProxy}
 */
export class ChromeCleanupProxyImpl {
  /** @override */
  registerChromeCleanerObserver() {
    chrome.send('registerChromeCleanerObserver');
  }

  /** @override */
  startScanning(logsUploadEnabled, notificationEnabled) {
    // TODO(1087263): Send the |notificationEnabled| parameter which indicates
    // if a completion dialog should be shown once the scan completed.
    chrome.send('startScanning', [logsUploadEnabled]);
  }

  /** @override */
  startCleanup(logsUploadEnabled) {
    chrome.send('startCleanup', [logsUploadEnabled]);
  }

  /** @override */
  restartComputer() {
    chrome.send('restartComputer');
  }

  /** @override */
  notifyShowDetails(enabled) {
    chrome.send('notifyShowDetails', [enabled]);
  }

  /** @override */
  notifyLearnMoreClicked() {
    chrome.send('notifyChromeCleanupLearnMoreClicked');
  }

  /** @override */
  getMoreItemsPluralString(numHiddenItems) {
    return sendWithPromise('getMoreItemsPluralString', numHiddenItems);
  }

  /** @override */
  getItemsToRemovePluralString(numItems) {
    return sendWithPromise('getItemsToRemovePluralString', numItems);
  }
}

addSingletonGetter(ChromeCleanupProxyImpl);
