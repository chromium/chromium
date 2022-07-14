// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class AudioAndCaptionsPageBrowserProxy {
  /**
   * Sets the startup sound to enabled.
   * @param {boolean} enabled
   */
  setStartupSoundEnabled(enabled) {}

  /**
   * Requests whether startup sound and tablet mode are enabled. Result
   * is returned by the 'initial-data-ready' WebUI listener event.
   */
  audioAndCaptionsPageReady() {}
}

/** @type {?AudioAndCaptionsPageBrowserProxy} */
let instance = null;

/**
 * @implements {AudioAndCaptionsPageBrowserProxy}
 */
export class AudioAndCaptionsPageBrowserProxyImpl {
  /** @return {!AudioAndCaptionsPageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new AudioAndCaptionsPageBrowserProxyImpl());
  }

  /** @param {!AudioAndCaptionsPageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  setStartupSoundEnabled(enabled) {
    chrome.send('setStartupSoundEnabled', [enabled]);
  }

  /** @override */
  audioAndCaptionsPageReady() {
    chrome.send('manageA11yPageReady');
  }
}
