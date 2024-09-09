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


  static getInstance(): NearbyPrefsBrowserProxy {
    return instance || (instance = new NearbyPrefsBrowserProxy());
  }
}

let instance: NearbyPrefsBrowserProxy|null = null;
