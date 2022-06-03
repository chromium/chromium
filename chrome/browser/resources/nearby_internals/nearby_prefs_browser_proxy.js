// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

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
}

addSingletonGetter(NearbyPrefsBrowserProxy);
