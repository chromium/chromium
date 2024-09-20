// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import type {Signals} from '../omnibox_internals.mojom-webui.js';
import {clamp, createEl, setFormattedClipboardForMl, signalNames} from '../omnibox_util.js';

import type {MlBrowserProxy} from './ml_browser_proxy.js';
/* eslint-disable-next-line @typescript-eslint/ban-ts-comment */
// @ts-ignore:next-line
import sheet from './ml_calculator.css' with {type : 'css'};
import {getTemplate} from './ml_calculator.html.js';

export class MlCalculatorElement extends CustomElement {
  private mlBrowserProxy_: MlBrowserProxy;
  private signalInputs: HTMLInputElement[];

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  connectedCallback() {
    this.signalInputs = signalNames.map(signalName => {
      const label = createEl(
          'label', this.getRequiredElement('#signals'), ['input-row'],
          signalName + ': ');
      const input = createEl('input', label);
      input.type = 'number';
      input.placeholder = 'null';
      input.addEventListener('input', () => this.update());
      return input;
    });

    this.getRequiredElement('#copy').addEventListener('click', async () => {
      const promise = setFormattedClipboardForMl(
          {score: this.score}, this.signals, window.location.href,
          await this.mlBrowserProxy_.modelVersion);
      this.dispatchEvent(new CustomEvent('copied', {detail: promise}));
    });

    this.getRequiredElement('#clear').addEventListener('click', () => {
      this.signalInputs.forEach(el => el.value = el.placeholder);
      this.update();
    });

    try {
      const urlSignals =
          new URLSearchParams(window.location.search).get('signals');
      if (urlSignals) {
        this.signals =
            MlCalculatorElement.parseSignalStrings(urlSignals.split(','));
      }
    } catch (e) {
    }
  }

  set mlBrowserProxy(mlBrowserProxy: MlBrowserProxy) {
    this.mlBrowserProxy_ = mlBrowserProxy;
    mlBrowserProxy.modelVersion.then(version => {
      createEl('a', this.getRequiredElement('#version'), [], version.string)
          .href = version.url;
    });
    this.update();
  }

  private static parseSignalStrings(signalStrings: string[]): Signals {
    assert(signalStrings.length === signalNames.length);
    return Object.fromEntries(
        signalStrings
            .map(str => {
              // Handle `''` and `null`; otherwise `Number()` would convert them
              // to `0`.
              if (!str) {
                return null;
              }
              const num = Number(str);
              return Number.isNaN(num) ?
                  null :
                  clamp(Math.floor(num), -(2 ** 31), 2 ** 31 - 1);
            })
            .map((signal, i) => [signalNames[i], signal]));
  }

  get signals(): Signals {
    return MlCalculatorElement.parseSignalStrings(
        this.signalInputs.map(input => input.value));
  }

  set signals(signals: Signals) {
    // Signals can be numbers, booleans, or null.
    Object.values(signals).forEach(
        (signal, i) => this.signalInputs[i]!.value =
            signal === null ? '' : String(Number(signal)));
    this.update();
  }

  private get score(): number {
    return Number(this.getRequiredElement('#score').textContent);
  }

  private set score(score: number) {
    this.getRequiredElement('#score').textContent = String(score);
  }

  private async update() {
    if (!this.mlBrowserProxy_) {
      return;
    }
    this.signalInputs.forEach(
        input => input.classList.toggle('empty', !!input.textContent));
    this.score = await this.mlBrowserProxy_.makeMlRequest(this.signals);
    window.history.replaceState(
        null, '', `?signals=${Object.values(this.signals)}`);
    this.dispatchEvent(new CustomEvent('updated'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ml-calculator': MlCalculatorElement;
  }
}

customElements.define('ml-calculator', MlCalculatorElement);
