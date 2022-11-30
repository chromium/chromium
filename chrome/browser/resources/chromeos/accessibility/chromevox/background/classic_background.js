// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script that runs on the background page.
 */
import {constants} from '../../common/constants.js';
import {AbstractEarcons} from '../common/abstract_earcons.js';
import {AbstractTts} from '../common/abstract_tts.js';
import {NavBraille} from '../common/braille/nav_braille.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {ExtensionBridge} from '../common/extension_bridge.js';
import {Msgs} from '../common/msgs.js';
import {QueueMode, TtsInterface, TtsSpeechProperties} from '../common/tts_interface.js';

import {BrailleBackground} from './braille/braille_background.js';
import {BrailleCaptionsBackground} from './braille/braille_captions_background.js';
import {ChromeVox} from './chromevox.js';
import {ChromeVoxState} from './chromevox_state.js';
import {ConsoleTts} from './console_tts.js';
import {ChromeVoxEditableTextBase, TypingEcho} from './editing/editable_text_base.js';
import {InjectedScriptLoader} from './injected_script_loader.js';
import {Output} from './output/output.js';
import {ChromeVoxPrefs} from './prefs.js';
import {TtsBackground} from './tts_background.js';

/**
 * This is the legacy ChromeVox background object.
 */
export class ChromeVoxBackground {
  constructor() {
    ChromeVoxBackground.readPrefs();

    this.addBridgeListener();

    // Build a regexp to match all allowed urls.
    let matches = [];
    try {
      matches = chrome.runtime.getManifest()['content_scripts'][0]['matches'];
    } catch (e) {
      throw new Error(
          'Unable to find content script matches entry in manifest.');
    }

    // Build one large regexp.
    const matchesRe = new RegExp(matches.join('|'));

    // Inject the content script into all running tabs allowed by the
    // manifest. This block is still necessary because the extension system
    // doesn't re-inject content scripts into already running tabs.
    chrome.windows.getAll({'populate': true}, windows => {
      for (let i = 0; i < windows.length; i++) {
        const tabs = windows[i].tabs.filter(tab => matchesRe.test(tab.url));
        InjectedScriptLoader.injectContentScript(tabs);
      }
    });
  }

  /**
   * @param {string} pref
   * @param {Object|boolean|number|string} value
   * @param {boolean} announce
   */
  static setPref(pref, value, announce) {
    if (pref === 'earcons') {
      AbstractEarcons.enabled = Boolean(value);
    } else if (pref === 'sticky' && announce) {
      if (typeof (value) !== 'boolean') {
        throw new Error('Unexpected sticky mode value ' + value);
      }
      chrome.accessibilityPrivate.setKeyboardListener(true, Boolean(value));
      new Output()
          .withInitialSpeechProperties(AbstractTts.PERSONALITY_ANNOTATION)
          .withString(
              value ? Msgs.getMsg('sticky_mode_enabled') :
                      Msgs.getMsg('sticky_mode_disabled'))
          .go();
    } else if (pref === 'typingEcho' && announce) {
      let announceStr = '';
      switch (value) {
        case TypingEcho.CHARACTER:
          announceStr = Msgs.getMsg('character_echo');
          break;
        case TypingEcho.WORD:
          announceStr = Msgs.getMsg('word_echo');
          break;
        case TypingEcho.CHARACTER_AND_WORD:
          announceStr = Msgs.getMsg('character_and_word_echo');
          break;
        case TypingEcho.NONE:
          announceStr = Msgs.getMsg('none_echo');
          break;
        default:
          break;
      }
      if (announceStr) {
        new Output()
            .withInitialSpeechProperties(AbstractTts.PERSONALITY_ANNOTATION)
            .withString(announceStr)
            .go();
      }
    } else if (pref === 'brailleCaptions') {
      BrailleCaptionsBackground.setActive(Boolean(value));
    } else if (pref === 'position') {
      ChromeVox.position =
          /** @type {Object<string, constants.Point>} */ (JSON.parse(
              /** @type {string} */ (value)));
    }
    ChromeVoxPrefs.instance.setPref(pref, value);
    ChromeVoxBackground.readPrefs();
  }

  /**
   * Read and apply preferences that affect the background context.
   */
  static readPrefs() {
    const prefs = ChromeVoxPrefs.instance.getPrefs();
    ChromeVoxEditableTextBase.useIBeamCursor =
        (prefs['useIBeamCursor'] === 'true');
    ChromeVox.isStickyPrefOn = (prefs['sticky'] === 'true');
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

  /**
   * Listen for connections from our content script bridges, and dispatch the
   * messages to the proper destination.
   */
  addBridgeListener() {
    ExtensionBridge.addMessageListener((msg, port) => {
      if (msg['target'] !== 'TTS') {
        return;
      }

      try {
        this.onTtsMessage(msg);
      } catch (err) {
        console.log(err);
      }
    });
  }

  /** Initializes classic background object. */
  static init() {
    const background = new ChromeVoxBackground();
  }
}
