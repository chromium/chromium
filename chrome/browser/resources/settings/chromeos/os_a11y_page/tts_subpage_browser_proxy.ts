// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface TtsSubpageBrowserProxy {
  /**
   * Requests the updated voice data. Returned by the 'all-voice-data-updated'
   * WebUI Listener event.
   */
  getAllTtsVoiceData(): void;

  /**
   * Requests the updated extensions. Returned by the 'tts-extensions-updated'
   * WebUI Listener event.
   */
  getTtsExtensions(): void;

  /**
   * Requests the tts preview. Returns a success boolean in the
   * 'tts-preview-state-changed' WebUI Listener event.
   */
  previewTtsVoice(previewText: string, previewVoice: string): void;

  /**
   * Awakens the tts engine.
   */
  wakeTtsEngine(): void;

  /**
   * Triggers the TtsPlatform to update its list of voices and relay that update
   * through VoicesChanged.
   */
  refreshTtsVoices(): void;
}

let instance: TtsSubpageBrowserProxy|null = null;

export class TtsSubpageBrowserProxyImpl implements TtsSubpageBrowserProxy {
  static getInstance(): TtsSubpageBrowserProxy {
    return instance || (instance = new TtsSubpageBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: TtsSubpageBrowserProxy): void {
    instance = obj;
  }

  getAllTtsVoiceData(): void {
    chrome.send('getAllTtsVoiceData');
  }

  getTtsExtensions(): void {
    chrome.send('getTtsExtensions');
  }

  previewTtsVoice(previewText: string, previewVoice: string): void {
    chrome.send('previewTtsVoice', [previewText, previewVoice]);
  }

  wakeTtsEngine(): void {
    chrome.send('wakeTtsEngine');
  }

  refreshTtsVoices(): void {
    chrome.send('refreshTtsVoices');
  }
}
