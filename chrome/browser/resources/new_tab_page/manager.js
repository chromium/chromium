// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

class ManagerUI extends PolymerElement {
  static get is() {
    return 'manager-ui';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();
    const browserProxy = BrowserProxy.getInstance();
    /** @private {newTabPage.mojom.PageCallbackRouter} */
    this.mojoEventTarget_ = browserProxy.callbackRouter;
    /** @private {newTabPage.mojom.PageHandlerInterface} */
    this.mojoHandler_ = browserProxy.handler;
  }
}

customElements.define(ManagerUI.is, ManagerUI);
