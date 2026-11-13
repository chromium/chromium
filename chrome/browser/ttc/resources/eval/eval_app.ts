// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../eval_harness.mojom-webui.js';
import type {TurnResult} from '../eval_harness.mojom-webui.js';

export class EvalAppElement extends CrLitElement {
  static get is() {
    return 'ttc-eval-app';
  }

  private get handler_() {
    return browserProxyFactory.getInstance().handler;
  }

  private isTestApiEnabled_(): boolean {
    return loadTimeData.valueExists('enableTtcTestApi') &&
        loadTimeData.getBoolean('enableTtcTestApi');
  }

  async start(): Promise<boolean> {
    if (!this.isTestApiEnabled_()) {
      throw new Error('TTC Test API is not enabled.');
    }
    const response = await this.handler_.startSession();
    return response.success;
  }

  end(): void {
    if (!this.isTestApiEnabled_()) {
      throw new Error('TTC Test API is not enabled.');
    }
    this.handler_.closeSession();
  }

  async sendText(text: string): Promise<TurnResult> {
    if (!this.isTestApiEnabled_()) {
      throw new Error('TTC Test API is not enabled.');
    }
    const response = await this.handler_.sendText(text);
    return response.result;
  }

  async sendAudio(pcm16Audio: Uint8Array): Promise<TurnResult> {
    if (!this.isTestApiEnabled_()) {
      throw new Error('TTC Test API is not enabled.');
    }
    const response = await this.handler_.sendAudio(Array.from(pcm16Audio));
    return response.result;
  }

  // Makes the session see the user as pressing at (x, y), in DIPs relative to
  // the top left of the focused tab's contents, until the next turn ends. A
  // null `x` or a missing `y` forgets the press.
  async setCursor(x: number|null, y?: number): Promise<boolean> {
    if (!this.isTestApiEnabled_()) {
      throw new Error('TTC Test API is not enabled.');
    }
    const point = (x !== null && y !== undefined) ? {x, y} : null;
    const response = await this.handler_.setPointerLocation(point);
    return response.success;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ttc-eval-app': EvalAppElement;
  }
}

customElements.define(EvalAppElement.is, EvalAppElement);
