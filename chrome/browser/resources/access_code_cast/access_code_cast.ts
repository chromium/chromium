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
  CODE_INPUT,
  QR_INPUT,
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
    this.setState(PageState.CODE_INPUT);
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

  switchToCodeInput() {
    this.setState(PageState.CODE_INPUT);
  }

  switchToQrInput() {
    this.setState(PageState.QR_INPUT);
  }

  private setState(state: PageState) {
    this.$.codeInputView.hidden = state !== PageState.CODE_INPUT;
    this.$.castButton.hidden = state !== PageState.CODE_INPUT;
    this.$.qrInputView.hidden = state !== PageState.QR_INPUT;
    this.$.backButton.hidden = state !== PageState.QR_INPUT;
  }
}

customElements.define(AccessCodeCastElement.is, AccessCodeCastElement);
