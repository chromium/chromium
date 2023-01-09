// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Text-To-Speech commands to Chrome's native TTS
 * extension API.
 */

import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {TtsInterface} from '../common/tts_interface.js';

import {ChromeVox} from './chromevox.js';
import {CompositeTts} from './composite_tts.js';
import {ConsoleTts} from './console_tts.js';
import {PrimaryTts} from './primary_tts.js';

const Action = BridgeConstants.TtsBackground.Action;
const TARGET = BridgeConstants.TtsBackground.TARGET;

/** This class broadly handles TTS within the background context. */
export class TtsBackground {
  static init() {
    TtsBackground.consoleTts_ = new ConsoleTts();
    TtsBackground.primaryTts_ = new PrimaryTts();
    TtsBackground.compositeTts_ = new CompositeTts()
                                      .add(TtsBackground.primary)
                                      .add(TtsBackground.console);

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
    if (!TtsBackground.compositeTts_) {
      throw new Error(
          'Cannot access composite TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.compositeTts_;
  }

  /** @return {!ConsoleTts} */
  static get console() {
    if (!TtsBackground.consoleTts_) {
      throw new Error(
          'Cannot access console TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.consoleTts_;
  }

  /** @return {!PrimaryTts} */
  static get primary() {
    if (!TtsBackground.primaryTts_) {
      throw new Error(
          'Cannot access primary TTS before TtsBackground has been ' +
          'initialized.');
    }
    return TtsBackground.primaryTts_;
  }
}

/** @private {CompositeTts} */
TtsBackground.compositeTts_;
/** @private {ConsoleTts} */
TtsBackground.consoleTts_;
/** @private {PrimaryTts} */
TtsBackground.primaryTts_;
