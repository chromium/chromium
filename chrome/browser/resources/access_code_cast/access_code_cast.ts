// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PageCallbackRouter} from './access_code_cast.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';

declare const chrome: any;

class AccessCodeCastElement extends PolymerElement {
  static get is() {
    return 'access-code-cast-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  private listenerIds: Array<number>;
  private router: PageCallbackRouter;

  constructor() {
    super();
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(id => this.router.removeListener(id));
  }

  close() {
    chrome.send('dialogClose');
  }
}

customElements.define(AccessCodeCastElement.is, AccessCodeCastElement);
