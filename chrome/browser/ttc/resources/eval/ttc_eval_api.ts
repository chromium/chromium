// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import type {HarnessPageCallbackRouter, HarnessPageHandlerRemote, TurnResult} from '../eval_harness.mojom-webui.js';

export interface TtcWindowApi {
  start: () => Promise<boolean>;
  end: () => void;
  sendText: (text: string) => Promise<TurnResult>;
  sendAudio: (pcm16Audio: Uint8Array) => Promise<TurnResult>;
}

/**
 * Exposes `window.ttc = { start, end, sendText, sendAudio }` on `eval.html`
 * when `--enable-ttc-test-api` is active, delegating turn execution, audio
 * injection, and observation to `HarnessPageHandler`.
 */
export class TtcEvalApi {
  private readonly handler_: HarnessPageHandlerRemote;

  constructor(
      handler: HarnessPageHandlerRemote, _router?: HarnessPageCallbackRouter) {
    this.handler_ = handler;
  }

  install(): void {
    const isTestApiEnabled = loadTimeData.valueExists('enableTtcTestApi') &&
        loadTimeData.getBoolean('enableTtcTestApi');
    if (!isTestApiEnabled) {
      return;
    }

    const api: TtcWindowApi = {
      start: () => this.start(),
      end: () => this.end(),
      sendText: (text: string) => this.sendText(text),
      sendAudio: (pcm16Audio: Uint8Array) => this.sendAudio(pcm16Audio),
    };

    (window as unknown as {ttc?: TtcWindowApi}).ttc = api;
  }

  async start(): Promise<boolean> {
    const response = await this.handler_.startSession();
    return response.success;
  }

  end(): void {
    this.handler_.closeSession();
  }

  async sendText(text: string): Promise<TurnResult> {
    const response = await this.handler_.sendText(text);
    return response.result;
  }

  async sendAudio(pcm16Audio: Uint8Array): Promise<TurnResult> {
    const response = await this.handler_.sendAudio(Array.from(pcm16Audio));
    return response.result;
  }
}
