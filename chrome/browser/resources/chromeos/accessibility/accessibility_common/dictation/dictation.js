// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Main class for the Chrome OS dictation feature.
 * Please note: this is being developed behind the flag
 * --enable-experimental-accessibility-dictation-extension
 */
export class Dictation {
  constructor() {
    chrome.accessibilityPrivate.onToggleDictation.addListener(
        this.onToggleDictation_.bind(this));
  }

  /**
   * Called when Dictation is toggled.
   * @param {boolean} activated Whether Dictation was just activated.
   * @private
   */
  onToggleDictation_(activated) {
    if (activated) {
      // Dictation as a JS extension isn't actually implemented yet, so just
      // turn off again.
      chrome.accessibilityPrivate.toggleDictation();
    }
  }
}
