// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PageHandler, PageHandlerInterface, ZeroTrustState} from './connectors_internals.mojom-webui.js';

interface ZeroTrustConnectorElement {
  $: {enabledString: HTMLSpanElement};
}

class ZeroTrustConnectorElement extends PolymerElement {
  static get is() {
    return 'zero-trust-connector';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      enabledString: String,
    };
  }

  private readonly pageHandler: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler = PageHandler.getRemote();

    this.fetchZeroTrustValues().then(state => this.setZeroTrustValues(state));
  }

  private setZeroTrustValues(state: ZeroTrustState|undefined) {
    if (!state) {
      this.$.enabledString.innerText = 'error';
      return;
    }

    this.$.enabledString.innerText = `${state.isEnabled}`;
  }

  private async fetchZeroTrustValues(): Promise<ZeroTrustState|undefined> {
    return this.pageHandler.getZeroTrustState().then(
        (response: {state: ZeroTrustState}) => response && response.state,
        (e: object) => {
          console.log(`fetchZeroTrustValues failed: ${JSON.stringify(e)}`);
          return undefined;
        });
  }
}

customElements.define(ZeroTrustConnectorElement.is, ZeroTrustConnectorElement);