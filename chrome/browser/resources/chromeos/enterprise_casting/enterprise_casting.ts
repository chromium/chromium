// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {PageCallbackRouter} from './enterprise_casting.mojom-webui.js';

class EnterpriseCastingElement extends PolymerElement {
  static get is() {
    return 'enterprise-casting';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      pin: String,
    };
  }

  private pin: string;
  private listenerIds: Array<number>;
  private router: PageCallbackRouter;

  constructor() {
    super();
    this.pin = '';
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;
  }

  connectedCallback() {
    super.connectedCallback();
    this.listenerIds.push(
        this.router.setPin.addListener(this.setPin.bind(this)));
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(id => this.router.removeListener(id));
  }

  requestPin() {
    BrowserProxy.getInstance().handler.updatePin();
  }

  private setPin(pin: string) {
    if (this.pin != pin) {
      this.pin = pin;
    }
  }
}

customElements.define(EnterpriseCastingElement.is, EnterpriseCastingElement);
