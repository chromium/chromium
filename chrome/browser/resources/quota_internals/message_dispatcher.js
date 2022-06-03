// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Bridge between the browser and the page.
 * In this file:
 *   * define interface to request data from the browser.
 */

/** Post requestInfo message to Browser. */
export function requestInfo() {
  chrome.send('requestInfo');
}

/**
 * Post triggerStoragePressure message to Browser.
 * @param {string} origin
 */
export function triggerStoragePressure(origin) {
  chrome.send('triggerStoragePressure', [origin]);
}
