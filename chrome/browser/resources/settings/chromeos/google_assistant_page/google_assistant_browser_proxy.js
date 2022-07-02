// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the google assistant section
 * to interact with the browser.
 */

  /** @interface */
export class GoogleAssistantBrowserProxy {
  /** Launches into the Google Assistant app settings. */
  showGoogleAssistantSettings() {}

  /** Retrain the Assistant voice model. */
  retrainAssistantVoiceModel() {}

  /** Sync the voice model status. */
  syncVoiceModelStatus() {}
}

/** @type {?GoogleAssistantBrowserProxy} */
let instance = null;

/** @implements {GoogleAssistantBrowserProxy} */
export class GoogleAssistantBrowserProxyImpl {
  /** @return {!GoogleAssistantBrowserProxy} */
  static getInstance() {
    return instance || (instance = new GoogleAssistantBrowserProxyImpl());
  }

  /** @param {!GoogleAssistantBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  showGoogleAssistantSettings() {
    chrome.send('showGoogleAssistantSettings');
  }

  /** @override */
  retrainAssistantVoiceModel() {
    chrome.send('retrainAssistantVoiceModel');
  }

  /** @override */
  syncVoiceModelStatus() {
    chrome.send('syncVoiceModelStatus');
  }
}
