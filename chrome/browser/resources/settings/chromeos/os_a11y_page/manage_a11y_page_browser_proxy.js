// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class ManageA11yPageBrowserProxy {
  /**
   * Opens the options page for Chrome Vox.
   */
  showChromeVoxSettings() {}

  /**
   * Opens the options page for select to speak.
   */
  showSelectToSpeakSettings() {}

  /**
   * Sets the startup sound to enabled.
   * @param {boolean} enabled
   */
  setStartupSoundEnabled(enabled) {}

  /**
   * Records the value of the show shelf navigation button.
   * @param {boolean} enabled
   */
  recordSelectedShowShelfNavigationButtonValue(enabled) {}

  /**
   * Requests whether startup sound and tablet mode are enabled. Result
   * is returned by the 'initial-data-ready' WebUI listener event.
   */
  manageA11yPageReady() {}

  /**
   * Opens the ChromeVox tutorial.
   */
  showChromeVoxTutorial() {}
}

/** @type {?ManageA11yPageBrowserProxy} */
let instance = null;

/**
 * @implements {ManageA11yPageBrowserProxy}
 */
export class ManageA11yPageBrowserProxyImpl {
  /** @return {!ManageA11yPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new ManageA11yPageBrowserProxyImpl());
  }

  /** @param {!ManageA11yPageBrowserProxy} obj */
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
  setStartupSoundEnabled(enabled) {
    chrome.send('setStartupSoundEnabled', [enabled]);
  }

  /** @override */
  recordSelectedShowShelfNavigationButtonValue(enabled) {
    chrome.send('recordSelectedShowShelfNavigationButtonValue', [enabled]);
  }

  /** @override */
  manageA11yPageReady() {
    chrome.send('manageA11yPageReady');
  }

  /** @override */
  showChromeVoxTutorial() {
    chrome.send('showChromeVoxTutorial');
  }
}
