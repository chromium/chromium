// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * JavaScript hooks into the native WebUI handler to communicate with C++ about
 * Nearby prefs.
 */
export class NearbyPrefsBrowserProxy {
  /**
   * Tells C++ side to clear Nearby Prefs.
   */
  clearNearbyPrefs() {
    chrome.send('clearNearbyPrefs');
  }

  /** @return {!NearbyPrefsBrowserProxy} */
  static getInstance() {
    return instance || (instance = new NearbyPrefsBrowserProxy());
  }
}

/** @type {?NearbyPrefsBrowserProxy} */
let instance = null;
