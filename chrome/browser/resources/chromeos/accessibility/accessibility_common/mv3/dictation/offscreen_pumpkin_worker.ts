// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OffscreenCommandType} from '../offscreen_command_type.js';

import * as PumpkinConstants from './parse/pumpkin/pumpkin_constants.js';

const SANDBOXED_PUMPKIN_TAGGER_JS_FILE =
    'dictation/parse/sandboxed_pumpkin_tagger.js';

/**
 * Offscreen way to communicate to pumpkin via worker.
 */
class OffscreenPumpkinWorker {
  private worker_: Worker|null = null;

  static instance?: OffscreenPumpkinWorker;

  static init(): void {
    if (OffscreenPumpkinWorker.instance) {
      throw 'Error: trying to create two instances of singleton ' +
          'OffscreenPumpkinWorker.';
    }
    OffscreenPumpkinWorker.instance = new OffscreenPumpkinWorker();
  }

  constructor() {
    chrome.runtime.onMessage.addListener(
        (message: any|undefined, _sender: chrome.runtime.MessageSender) =>
            this.handleMessageFromServiceWorker_(message));
  }

  private handleMessageFromServiceWorker_(message: any|undefined): boolean {
    switch (message['command']) {
      case OffscreenCommandType.DICTATION_PUMPKIN_INSTALL:
        this.worker_ =
            new Worker(SANDBOXED_PUMPKIN_TAGGER_JS_FILE, {type: 'module'});
        this.worker_.onmessage = (message) =>
            chrome.runtime.sendMessage(undefined, {
              command: OffscreenCommandType.DICTATION_PUMPKIN_RECEIVE,
              fromPumpkinTagger: message.data
            });
        break;
      case OffscreenCommandType.DICTATION_PUMPKIN_SEND:
        this.sendToSandboxedPumpkinTagger_(message['toPumpkinTagger'])
        break;
    }
    return false;
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

OffscreenPumpkinWorker.init();
