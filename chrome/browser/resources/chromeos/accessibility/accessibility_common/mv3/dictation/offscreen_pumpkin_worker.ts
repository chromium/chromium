// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Messenger} from '../messenger.js';
import {OffscreenCommandType} from '../offscreen_command_type.js';

import * as PumpkinConstants from './parse/pumpkin/pumpkin_constants.js';


/**
 * Offscreen way to communicate to pumpkin via a sandboxed iframe.
 */
class OffscreenPumpkinWorker {
  private sandbox_: HTMLIFrameElement;

  constructor() {
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PUMPKIN_INSTALL,
        () => this.createSandboxedPumpkinTagger_());
    Messenger.registerHandler(
        OffscreenCommandType.DICTATION_PUMPKIN_SEND,
        (message: any|undefined) =>
            this.sendToSandboxedPumpkinTagger_(message.toPumpkinTagger));

    window.addEventListener(
        'message', (event) => this.onSandboxMessage_(event));
    this.sandbox_ = document.getElementById('sandboxed-pumpkin-tagger') as
        HTMLIFrameElement;
  }

  private async createSandboxedPumpkinTagger_() {
    this.sandbox_.src = 'sandboxed_pumpkin_tagger.html';
  }

  private async sendToSandboxedPumpkinTagger_(
      toPumpkinTagger: PumpkinConstants.ToPumpkinTagger) {
    // Deseriazlie ArrayBuffer fields in pumpkinData before sending it to
    // tagger worker.
    // 1. Traverse the `pumpkinData` object and convert each value (an array
    // [v1, v2, ...]) back into a Uint8Array, then extract its underlying
    // ArrayBuffer.
    // 2. Reconstruct a new object with the original keys and the deserialized
    // values.
    const pumpkinData = toPumpkinTagger.pumpkinData ?
        Object.fromEntries(await Promise.all(
            Object.entries(toPumpkinTagger.pumpkinData)
                .map(async ([key, array]) => {
                  return [key, await Messenger.base64ToArrayBuffer(array)];
                }))) :
        null;

    this.sandbox_.contentWindow!.postMessage(
        {...toPumpkinTagger, pumpkinData}, '*');
  }

  /**
   * Handle of messages from the sandboxed tagger.
   */
  private onSandboxMessage_(event: MessageEvent) {
    if (event.source !== this.sandbox_.contentWindow) {
      console.error(`Reject sandbox message: bad event source`);
      return;
    }

    Messenger.send(
        OffscreenCommandType.DICTATION_PUMPKIN_RECEIVE,
        {fromPumpkinTagger: event.data});
  }
}

export {OffscreenPumpkinWorker};
