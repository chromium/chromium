// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a global object that holds references to the three
 * different output engines.
 */
import {BridgeHelper} from '/common/bridge_helper.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {NavBraille} from '../common/braille/nav_braille.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import type {EarconId} from '../common/earcon_id.js';
import {Spannable} from '../common/spannable.js';
import type {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';

import type {AbstractEarcons} from './abstract_earcons.js';
import {BrailleCommandHandler} from './braille/braille_command_handler.js';
import type {BrailleInterface} from './braille/braille_interface.js';
import {ChromeVoxState} from './chromevox_state.js';
import type {TtsInterface} from './tts_interface.js';

/** A central access point for the different modes of output. */
export class ChromeVox {
  static braille: BrailleInterface;
  static earcons: AbstractEarcons;
  static tts: TtsInterface;
}

// Braille bridge functions.
BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET,
    BridgeConstants.Braille.Action.BACK_TRANSLATE,
    (cells: ArrayBuffer) =>
        Promise.resolve(ChromeVox.braille?.backTranslate(cells)));

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.SET_BYPASS,
    async (bypass: boolean) => {
      await ChromeVoxState.ready();
      BrailleCommandHandler.setBypass(bypass);
    });

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.PAN_LEFT,
    () => ChromeVox.braille?.panLeft());

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.PAN_RIGHT,
    () => ChromeVox.braille?.panRight());

BridgeHelper.registerHandler(
    BridgeConstants.Braille.TARGET, BridgeConstants.Braille.Action.WRITE,
    (text: Spannable) =>
        ChromeVox.braille?.write(new NavBraille({text: new Spannable(text)})));

// Earcon bridge functions.
BridgeHelper.registerHandler(
    BridgeConstants.Earcons.TARGET,
    BridgeConstants.Earcons.Action.CANCEL_EARCON,
    (earconId: EarconId) => ChromeVox.earcons?.cancelEarcon(earconId));

BridgeHelper.registerHandler(
    BridgeConstants.Earcons.TARGET, BridgeConstants.Earcons.Action.PLAY_EARCON,
    (earconId: EarconId) => ChromeVox.earcons?.playEarcon(earconId));

// TTS bridge functions.
BridgeHelper.registerHandler(
    BridgeConstants.TtsBackground.TARGET,
    BridgeConstants.TtsBackground.Action.SPEAK,
    (text: string, queueMode: QueueMode, properties: TtsSpeechProperties) =>
        ChromeVox.tts?.speak(text, queueMode, properties));

TestImportManager.exportForTesting(['ChromeVox', ChromeVox]);
