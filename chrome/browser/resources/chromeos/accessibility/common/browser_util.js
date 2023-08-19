// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for interacting with browser windows.
 */

export class BrowserUtil {
  /**
   * Opens a URL in the user's preferred browser (Lacros if enabled, Ash
   * otherwise). If a feature needs to always open in the Ash browser, for
   * example to show an extension page, it should not use this method.
   * @param {string} url The URL to open.
   */
  static async openBrowserUrl(url) {
    if (BrowserUtil.isLacrosEnabled_ === null) {
      // Cache the value on first use. This will not change after Chrome OS
      // is already running.
      BrowserUtil.isLacrosEnabled_ = await new Promise(
          resolve => chrome.accessibilityPrivate.isLacrosPrimary(resolve));
    }

    if (BrowserUtil.isLacrosEnabled_) {
      globalThis.open(url, '_blank');
      return;
    }

    chrome.windows.getAll((windows) => {
      if (windows.length > 0) {
        // Open in existing window.
        chrome.tabs.create({url});
      } else {
        // No window open, cannot use chrome.tabs API (chrome.tabs.create
        // would error).
        chrome.windows.create({url});
      }
    });
  }
}

/**
 * Cached value of AccessibilityPrivate.isLacrosPrimary,
 * null if it hasn't been fetched yet.
 * @private {?boolean}
 */
BrowserUtil.isLacrosEnabled_ = null;
