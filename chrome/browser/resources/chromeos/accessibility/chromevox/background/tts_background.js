// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Text-To-Speech commands to Chrome's native TTS
 * extension API.
 */

import {constants} from '../../common/constants.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Msgs} from '../common/msgs.js';
import {SettingsManager} from '../common/settings_manager.js';
import {TtsInterface} from '../common/tts_interface.js';
import {Personality, QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import {CompositeTts} from './composite_tts.js';
import {ConsoleTts} from './console_tts.js';
import {PrimaryTts} from './primary_tts.js';

const Action = BridgeConstants.TtsBackground.Action;
const TARGET = BridgeConstants.TtsBackground.TARGET;

/** This class broadly handles TTS within the background context. */
export class TtsBackground {
  /** @private */
  constructor() {
    /** @private {!ConsoleTts} */
    this.consoleTts_ = new ConsoleTts();
    /** @private {!PrimaryTts} */
    this.primaryTts_ = new PrimaryTts();

    /** @private {!CompositeTts} */
    this.compositeTts_ =
        new CompositeTts().add(this.primaryTts_).add(this.consoleTts_);
  }
  static init() {
    TtsBackground.instance = new TtsBackground();
    ChromeVox.tts = TtsBackground.composite;

    BridgeHelper.registerHandler(
        TARGET, Action.UPDATE_PUNCTUATION_ECHO,
        echo => TtsBackground.primary.updatePunctuationEcho(echo));
    BridgeHelper.registerHandler(
        TARGET, Action.GET_CURRENT_VOICE,
        () => TtsBackground.primary.currentVoice);
  }

  /** @return {!CompositeTts} */
  static get composite() {
    if (!TtsBackground.instance) {
      throw new Error(
          'Cannot access composite TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.instance.compositeTts_;
  }

  /** @return {!ConsoleTts} */
  static get console() {
    if (!TtsBackground.instance) {
      throw new Error(
          'Cannot access console TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.instance.consoleTts_;
  }

  /** @return {!PrimaryTts} */
  static get primary() {
    if (!TtsBackground.instance) {
      throw new Error(
          'Cannot access primary TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.instance.primaryTts_;
  }

  static resetTextToSpeechSettings() {
    const rate = ChromeVox.tts.getDefaultProperty('rate');
    const pitch = ChromeVox.tts.getDefaultProperty('pitch');
    const volume = ChromeVox.tts.getDefaultProperty('volume');
    chrome.settingsPrivate.setPref('settings.tts.speech_rate', rate);
    chrome.settingsPrivate.setPref('settings.tts.speech_pitch', pitch);
    chrome.settingsPrivate.setPref('settings.tts.speech_volume', volume);
    SettingsManager.set('voiceName', constants.SYSTEM_VOICE);
    TtsBackground.primary.updateVoice('', () => {
      // Ensure this announcement doesn't get cut off by speech triggered by
      // updateFromPrefs_().
      const speechProperties = {...Personality.ANNOTATION};
      speechProperties.doNotInterrupt = true;

      ChromeVox.tts.speak(
          Msgs.getMsg('announce_tts_default_settings'), QueueMode.FLUSH,
          new TtsSpeechProperties(speechProperties));
    });
  }
}
