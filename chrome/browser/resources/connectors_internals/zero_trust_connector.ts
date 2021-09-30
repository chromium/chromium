// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {PageHandler, PageHandlerInterface, ZeroTrustState} from './connectors_internals.mojom-webui.js';

export class ZeroTrustConnectorElement extends CustomElement {
  static get is() {
    return 'zero-trust-connector';
  }

  static get template() {
    return `{__html_template__}`;
  }

  public set enabledString(str: string) {
    const strEl = (this.$('#enabled-string') as HTMLElement);
    if (strEl) {
      strEl.innerText = str;
    } else {
      console.error('Could not find #enabled-string element.');
    }
  }

  private _signalsString: string = '';
  public set signalsString(str: string) {
    const signalsEl = (this.$('#signals') as HTMLElement);
    if (signalsEl) {
      signalsEl.innerText = str;
      this._signalsString = str;
    } else {
      console.error('Could not find #signals element.');
    }
  }

  public get copyButton(): HTMLButtonElement|undefined {
    return this.$('#copy-signals') as HTMLButtonElement;
  }

  public get signalsString(): string {
    return this._signalsString;
  }

  private readonly pageHandler: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler = PageHandler.getRemote();

    this.fetchZeroTrustValues()
        .then(state => this.setZeroTrustValues(state))
        .then(() => {
          const copyButton = this.copyButton;
          if (copyButton) {
            copyButton.addEventListener(
                'click', () => this.copySignals(copyButton));
          }
        });
  }

  private setZeroTrustValues(state: ZeroTrustState|undefined) {
    if (!state) {
      this.enabledString = 'error';
      return;
    }

    this.enabledString = `${state.isEnabled}`;

    // Pretty print the dictionary as a JSON string.
    this.signalsString = JSON.stringify(state.signalsDictionary, null, 2);
  }

  private async fetchZeroTrustValues(): Promise<ZeroTrustState|undefined> {
    return this.pageHandler.getZeroTrustState().then(
        (response: {state: ZeroTrustState}) => response && response.state,
        (e: object) => {
          console.log(`fetchZeroTrustValues failed: ${JSON.stringify(e)}`);
          return undefined;
        });
  }

  private async copySignals(copyButton: HTMLButtonElement): Promise<void> {
    copyButton.disabled = true;
    navigator.clipboard.writeText(this.signalsString)
        .finally(() => copyButton.disabled = false);
  }
}

customElements.define(ZeroTrustConnectorElement.is, ZeroTrustConnectorElement);