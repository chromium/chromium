// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PageCallbackRouter} from './access_code_cast.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';

declare const chrome: any;

enum PageState {
  CODEINPUT,
  QRINPUT,
}

interface AccessCodeCastElement {
  $: {
    backButton: CrButtonElement;
    castButton: CrButtonElement;
    codeInputView: HTMLDivElement;
    qrInputView: HTMLDivElement;
  }
}

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

  ready() {
    super.ready();
    this.setState(PageState.CODEINPUT);
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(id => this.router.removeListener(id));
  }

  /**
   * -------------------------
   * Event handlers
   * -------------------------
   */

  close() {
    chrome.send('dialogClose');
  }

  switchToCodeInput() {
    this.setState(PageState.CODEINPUT);
  }

  switchToQrInput() {
    this.setState(PageState.QRINPUT);
  }

  /**
   * -------------------------
   * Helper functions
   * -------------------------
   */

  private setState(state: PageState) {
    this.$.codeInputView.hidden = state !== PageState.CODEINPUT;
    this.$.castButton.hidden = state !== PageState.CODEINPUT;
    this.$.qrInputView.hidden = state !== PageState.QRINPUT;
    this.$.backButton.hidden = state !== PageState.QRINPUT;
  }
}

customElements.define(AccessCodeCastElement.is, AccessCodeCastElement);
