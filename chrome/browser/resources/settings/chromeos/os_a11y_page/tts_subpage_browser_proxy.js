// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// clang-format on

/** @interface */
/* #export */ class TtsSubpageBrowserProxy {
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
}

/**
 * @implements {TtsSubpageBrowserProxy}
 */
/* #export */ class TtsSubpageBrowserProxyImpl {
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
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
cr.addSingletonGetter(TtsSubpageBrowserProxyImpl);