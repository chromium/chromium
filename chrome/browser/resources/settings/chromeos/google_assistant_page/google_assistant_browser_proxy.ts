// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the google assistant section
 * to interact with the browser.
 */

export interface GoogleAssistantBrowserProxy {
  /** Launches into the Google Assistant app settings. */
  showGoogleAssistantSettings(): void;

  /** Retrain the Assistant voice model. */
  retrainAssistantVoiceModel(): void;

  /** Sync the voice model status. */
  syncVoiceModelStatus(): void;
}

let instance: GoogleAssistantBrowserProxy|null = null;

export class GoogleAssistantBrowserProxyImpl implements
    GoogleAssistantBrowserProxy {
  static getInstance(): GoogleAssistantBrowserProxy {
    return instance || (instance = new GoogleAssistantBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: GoogleAssistantBrowserProxy): void {
    instance = obj;
  }

  showGoogleAssistantSettings() {
    chrome.send('showGoogleAssistantSettings');
  }

  retrainAssistantVoiceModel() {
    chrome.send('retrainAssistantVoiceModel');
  }

  syncVoiceModelStatus() {
    chrome.send('syncVoiceModelStatus');
  }
}
