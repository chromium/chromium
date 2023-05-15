// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script that runs on the background page.
 */
import {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import {InjectedScriptLoader} from './injected_script_loader.js';

/**
 * This is the legacy ChromeVox background object.
 */
export class ChromeVoxBackground {
  constructor() {
    this.injectContentScriptForGoogleDocs_();
  }

  /** @private */
  injectContentScriptForGoogleDocs_() {
    // Build a regexp to match all allowed urls.
    let matches = [];
    try {
      matches = chrome.runtime.getManifest()['content_scripts'][0]['matches'];
    } catch (e) {
      throw new Error(
          'Unable to find content script matches entry in manifest.');
    }

    // Build one large regexp.
    const docsRe = new RegExp(matches.join('|'));

    // Inject the content script into all running tabs allowed by the
    // manifest. This block is still necessary because the extension system
    // doesn't re-inject content scripts into already running tabs.
    chrome.windows.getAll({'populate': true}, windows => {
      for (let i = 0; i < windows.length; i++) {
        const tabs = windows[i].tabs.filter(tab => docsRe.test(tab.url));
        InjectedScriptLoader.injectContentScript(tabs);
      }
    });
  }

  /**
   * Called when a TTS message is received from a page content script.
   * @param {Object} msg The TTS message.
   */
  onTtsMessage(msg) {
    if (msg['action'] !== 'speak') {
      return;
    }
    // The only caller sending this message is a ChromeVox Classic api client.
    // Deny empty strings.
    if (msg['text'] === '') {
      return;
    }

    ChromeVox.tts.speak(
        msg['text'],
        /** @type {QueueMode} */ (msg['queueMode']),
        new TtsSpeechProperties(msg['properties']));
  }

  /** Initializes classic background object. */
  static init() {
    const background = new ChromeVoxBackground();
  }
}
