// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface SelectToSpeakSubpageBrowserProxy {
  /**
   * Requests the updated voice data. Returned by the
   * 'all-sts-voice-data-updated' WebUI Listener event.
   */
  getAllTtsVoiceData(): void;

  /**
   * Requests the app locale. Returned by the 'app-locale-updated' WebUI
   * Listener event.
   */
  getAppLocale(): void;

  /**
   * Requests the tts preview. Returns a success boolean in the
   * 'tts-preview-state-changed' WebUI Listener event.
   */
  previewTtsVoice(previewText: string, previewVoice: string): void;

  /**
   * Triggers the TtsPlatform to update its list of voices and relay that update
   * through VoicesChanged.
   */
  refreshTtsVoices(): void;
}

let instance: SelectToSpeakSubpageBrowserProxy|null = null;

export class SelectToSpeakSubpageBrowserProxyImpl implements
    SelectToSpeakSubpageBrowserProxy {
  static getInstance(): SelectToSpeakSubpageBrowserProxy {
    return instance || (instance = new SelectToSpeakSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: SelectToSpeakSubpageBrowserProxy): void {
    instance = obj;
  }

  getAllTtsVoiceData(): void {
    chrome.send('getAllTtsVoiceDataForSts');
  }

  getAppLocale(): void {
    chrome.send('getAppLocale');
  }

  previewTtsVoice(previewText: string, previewVoice: string): void {
    chrome.send('previewTtsVoiceForSts', [previewText, previewVoice]);
  }

  refreshTtsVoices(): void {
    chrome.send('refreshTtsVoices');
  }
}
