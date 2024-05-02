// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Text-To-Speech commands to Chrome's native TTS
 * extension API.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {constants} from '/common/constants.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {BridgeConstants} from '../common/bridge_constants.js';
import {Msgs} from '../common/msgs.js';
import {SettingsManager} from '../common/settings_manager.js';
import {Personality, QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import {ChromeVox} from './chromevox.js';
import {CompositeTts} from './composite_tts.js';
import {ConsoleTts} from './console_tts.js';
import {Output} from './output/output.js';
import {PrimaryTts} from './primary_tts.js';

const Action = BridgeConstants.TtsBackground.Action;
const TARGET = BridgeConstants.TtsBackground.TARGET;

/** This class broadly handles TTS within the background context. */
export class TtsBackground {
  static instance: TtsBackground;

  private compositeTts_: CompositeTts;
  private consoleTts_: ConsoleTts;
  private primaryTts_: PrimaryTts;

  private constructor() {
    this.consoleTts_ = new ConsoleTts();
    this.primaryTts_ = new PrimaryTts();
    this.compositeTts_ =
        new CompositeTts().add(this.primaryTts_).add(this.consoleTts_);
  }
  static init(): void {
    TtsBackground.instance = new TtsBackground();
    ChromeVox.tts = TtsBackground.composite;

    BridgeHelper.registerHandler(
        TARGET, Action.UPDATE_PUNCTUATION_ECHO,
        (echo: number) => TtsBackground.primary.updatePunctuationEcho(echo));
    BridgeHelper.registerHandler(
        TARGET, Action.GET_CURRENT_VOICE,
        () => TtsBackground.primary.currentVoice);
  }

  static get composite(): CompositeTts {
    if (!TtsBackground.instance) {
      throw new Error(
          'Cannot access composite TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.instance.compositeTts_;
  }

  static get console(): ConsoleTts {
    if (!TtsBackground.instance) {
      throw new Error(
          'Cannot access console TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.instance.consoleTts_;
  }

  static get primary(): PrimaryTts {
    if (!TtsBackground.instance) {
      throw new Error(
          'Cannot access primary TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.instance.primaryTts_;
  }

  static resetTextToSpeechSettings(): void {
    const rate = ChromeVox.tts.getDefaultProperty('rate');
    const pitch = ChromeVox.tts.getDefaultProperty('pitch');
    const volume = ChromeVox.tts.getDefaultProperty('volume');
    ChromeVox.tts.setProperty('rate', rate ? rate : 1);
    ChromeVox.tts.setProperty('pitch', pitch ? pitch : 1);
    ChromeVox.tts.setProperty('volume', volume ? volume : 1);
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

  /** Toggles speech on or off and announces the change. */
  static toggleSpeechWithAnnouncement(): void {
    const state = ChromeVox.tts.toggleSpeechOnOrOff();
    new Output().format(state ? '@speech_on' : '@speech_off').go();
  }
}

TestImportManager.exportForTesting(TtsBackground);
