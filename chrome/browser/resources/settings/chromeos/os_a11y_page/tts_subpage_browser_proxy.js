// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
export class TtsSubpageBrowserProxy {
  /**
   * Requests the updated voice data. Returned by the 'all-voice-data-updated'
   * WebUI Listener event.
   */
  getAllTtsVoiceData() {}

  /**
   * Requests the updated extensions. Returned by the 'tts-extensions-updated'
   * WebUI Listener event.
   */
  getTtsExtensions() {}

  /**
   * Requests the tts preview. Returns a success boolean in the
   * 'tts-preview-state-changed' WebUI Listener event.
   * @param {string} previewText
   * @param {String} previewVoice
   */
  previewTtsVoice(previewText, previewVoice) {}

  /**
   * Awakens the tts engine.
   */
  wakeTtsEngine() {}

  /**
   * Triggers the TtsPlatform to update its list of voices and relay that update
   * through VoicesChanged.
   */
  refreshTtsVoices() {}
}

/** @type {?TtsSubpageBrowserProxy} */
let instance = null;

/**
 * @implements {TtsSubpageBrowserProxy}
 */
export class TtsSubpageBrowserProxyImpl {
  /** @return {!TtsSubpageBrowserProxy} */
  static getInstance() {
    return instance || (instance = new TtsSubpageBrowserProxyImpl());
  }

  /** @param {!TtsSubpageBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }

  /** @override */
  getAllTtsVoiceData() {
    chrome.send('getAllTtsVoiceData');
  }

  /** @override */
  getTtsExtensions() {
    chrome.send('getTtsExtensions');
  }

  /** @override */
  previewTtsVoice(previewText, previewVoice) {
    chrome.send('previewTtsVoice', [previewText, previewVoice]);
  }

  /** @override */
  wakeTtsEngine() {
    chrome.send('wakeTtsEngine');
  }

  /** @override */
  refreshTtsVoices() {
    chrome.send('refreshTtsVoices');
  }
}
