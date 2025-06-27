// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Messenger} from '../messenger.js';
import {OffscreenCommandType} from '../offscreen_command_type.js';

import * as PumpkinConstants from './parse/pumpkin/pumpkin_constants.js';

const SANDBOXED_PUMPKIN_TAGGER_JS_FILE =
    'dictation/parse/sandboxed_pumpkin_tagger.js';

/**
 * Offscreen way to communicate to pumpkin via worker.
 */
class OffscreenPumpkinWorker {
  private worker_: Worker|null = null;

  constructor() {
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PUMPKIN_INSTALL,
        () => this.createSandboxedPumpkinTagger_());
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PUMPKIN_SEND,
        (message: any|undefined) =>
            this.sendToSandboxedPumpkinTagger_(message['toPumpkinTagger']));
  }

  private createSandboxedPumpkinTagger_() {
    this.worker_ =
        new Worker(SANDBOXED_PUMPKIN_TAGGER_JS_FILE, {type: 'module'});
    this.worker_.onmessage = (message) =>
        chrome.runtime.sendMessage(undefined, {
          command: OffscreenCommandType.DICTATION_PUMPKIN_RECEIVE,
          fromPumpkinTagger: message.data
        });
  }

  private sendToSandboxedPumpkinTagger_(
      toPumpkinTagger: PumpkinConstants.ToPumpkinTagger): void {
    if (!this.worker_) {
      throw new Error(
          `Worker not ready, cannot send command to SandboxedPumpkinTagger: ${
              toPumpkinTagger.type}`);
    }

    // Deseriazlie ArrayBuffer fields in pumpkinData before sending it to
    // tagger worker.
    // 1. Traverse the `pumpkinData` object and convert each value (an array
    // [v1, v2, ...]) back into a Uint8Array, then extract its underlying
    // ArrayBuffer.
    // 2. Reconstruct a new object with the original keys and the deserialized
    // values.
    toPumpkinTagger.pumpkinData = toPumpkinTagger.pumpkinData ?
        Object.fromEntries(
            Object.entries(toPumpkinTagger.pumpkinData)
                .map(([key, array]) => [key, new Uint8Array(array).buffer])) as
            PumpkinConstants.PumpkinData :
        null;

    this.worker_.postMessage(toPumpkinTagger);
  }
}

export {OffscreenPumpkinWorker};
