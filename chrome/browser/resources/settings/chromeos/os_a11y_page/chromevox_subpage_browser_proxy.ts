// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ChromeVoxSubpageBrowserProxy {
  /**
   * Requests the updated voice data. Returned by the 'all-voice-data-updated'
   * WebUI Listener event.
   */
  getAllTtsVoiceData(): void;

  /**
   * Triggers the TtsPlatform to update its list of voices and relay that update
   * through VoicesChanged.
   */
  refreshTtsVoices(): void;
}

let instance: ChromeVoxSubpageBrowserProxy|null = null;

export class ChromeVoxSubpageBrowserProxyImpl implements
    ChromeVoxSubpageBrowserProxy {
  static getInstance(): ChromeVoxSubpageBrowserProxy {
    return instance || (instance = new ChromeVoxSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: ChromeVoxSubpageBrowserProxy): void {
    instance = obj;
  }

  getAllTtsVoiceData(): void {
    chrome.send('getAllTtsVoiceData');
  }

  refreshTtsVoices(): void {
    chrome.send('refreshTtsVoices');
  }
}
