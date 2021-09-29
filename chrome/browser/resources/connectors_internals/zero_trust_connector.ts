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

  private readonly pageHandler: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler = PageHandler.getRemote();

    this.fetchZeroTrustValues().then(state => this.setZeroTrustValues(state));
  }

  private setZeroTrustValues(state: ZeroTrustState|undefined) {
    if (!state) {
      this.enabledString = 'error';
      return;
    }

    this.enabledString = `${state.isEnabled}`;
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