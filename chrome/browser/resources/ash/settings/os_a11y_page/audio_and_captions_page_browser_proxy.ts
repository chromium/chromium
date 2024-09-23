// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface AudioAndCaptionsPageBrowserProxy {
  /**
   * Sets the startup sound to enabled.
   */
  setStartupSoundEnabled(enabled: boolean): void;

  /**
   * Requests whether startup sound and tablet mode are enabled. Result
   * is returned by the 'initial-data-ready' WebUI listener event.
   */
  audioAndCaptionsPageReady(): void;

  /**
   * Request whether startup sound is enabled. Result is returned by the
   * 'startup-sound-setting-retrieved' WebUI listener event.
   */
  getStartupSoundEnabled(): void;

  /**
   * Requests the system preview the flash notification feature.
   */
  previewFlashNotification(): void;
}

let instance: AudioAndCaptionsPageBrowserProxy|null = null;

export class AudioAndCaptionsPageBrowserProxyImpl implements
    AudioAndCaptionsPageBrowserProxy {
  static getInstance(): AudioAndCaptionsPageBrowserProxy {
    return instance || (instance = new AudioAndCaptionsPageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: AudioAndCaptionsPageBrowserProxy): void {
    instance = obj;
  }

  setStartupSoundEnabled(enabled: boolean): void {
    chrome.send('setStartupSoundEnabled', [enabled]);
  }

  audioAndCaptionsPageReady(): void {
    chrome.send('manageA11yPageReady');
  }

  getStartupSoundEnabled(): void {
    chrome.send('getStartupSoundEnabled');
  }

  previewFlashNotification(): void {
    chrome.send('previewFlashNotification');
  }
}
