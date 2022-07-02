// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class TextToSpeechPageBrowserProxy {
  /**
   * Opens the options page for Chrome Vox.
   */
  showChromeVoxSettings() {}

  /**
   * Opens the options page for select to speak.
   */
  showSelectToSpeakSettings() {}

  /**
   * Opens the ChromeVox tutorial.
   */
  showChromeVoxTutorial() {}
}

/** @type {?TextToSpeechPageBrowserProxy} */
let instance = null;

/**
 * @implements {TextToSpeechPageBrowserProxy}
 */
export class TextToSpeechPageBrowserProxyImpl {
  /** @return {!TextToSpeechPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new TextToSpeechPageBrowserProxyImpl());
  }

  /** @param {!TextToSpeechPageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  showChromeVoxSettings() {
    chrome.send('showChromeVoxSettings');
  }

  /** @override */
  showSelectToSpeakSettings() {
    chrome.send('showSelectToSpeakSettings');
  }

  /** @override */
  showChromeVoxTutorial() {
    chrome.send('showChromeVoxTutorial');
  }
}