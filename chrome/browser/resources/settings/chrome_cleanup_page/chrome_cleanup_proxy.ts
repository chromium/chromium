// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface ChromeCleanupProxy {
  /**
   * Registers the current ChromeCleanupHandler as an observer of
   * ChromeCleanerController events.
   */
  registerChromeCleanerObserver(): void;

  /**
   * Starts scanning the user's computer.
   */
  startScanning(logsUploadEnabled: boolean): void;

  /**
   * Starts a cleanup on the user's computer.
   */
  startCleanup(logsUploadEnabled: boolean): void;

  /**
   * Restarts the user's computer.
   */
  restartComputer(): void;

  /**
   * Notifies Chrome that the state of the details section changed.
   */
  notifyShowDetails(enabled: boolean): void;

  /**
   * Notifies Chrome that the "learn more" link was clicked.
   */
  notifyLearnMoreClicked(): void;

  /**
   * Requests the plural string for the "show more" link in the detailed
   * view for either files to delete or registry keys.
   */
  getMoreItemsPluralString(numHiddenItems: number): Promise<string>;

  /**
   * Requests the plural string for the "items to remove" link in the detailed
   * view.
   */
  getItemsToRemovePluralString(numItems: number): Promise<string>;
}

export class ChromeCleanupProxyImpl implements ChromeCleanupProxy {
  registerChromeCleanerObserver() {
    chrome.send('registerChromeCleanerObserver');
  }

  startScanning(logsUploadEnabled: boolean) {
    chrome.send('startScanning', [logsUploadEnabled]);
  }

  startCleanup(logsUploadEnabled: boolean) {
    chrome.send('startCleanup', [logsUploadEnabled]);
  }

  restartComputer() {
    chrome.send('restartComputer');
  }

  notifyShowDetails(enabled: boolean) {
    chrome.send('notifyShowDetails', [enabled]);
  }

  notifyLearnMoreClicked() {
    chrome.send('notifyChromeCleanupLearnMoreClicked');
  }

  getMoreItemsPluralString(numHiddenItems: number) {
    return sendWithPromise('getMoreItemsPluralString', numHiddenItems);
  }

  getItemsToRemovePluralString(numItems: number) {
    return sendWithPromise('getItemsToRemovePluralString', numItems);
  }

  static getInstance(): ChromeCleanupProxy {
    return instance || (instance = new ChromeCleanupProxyImpl());
  }

  static setInstance(obj: ChromeCleanupProxy) {
    instance = obj;
  }
}

let instance: ChromeCleanupProxy|null = null;
