// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './code_input/code_input.js';

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PageCallbackRouter} from './access_code_cast.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {CodeInputElement} from './code_input/code_input.js';

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
    codeInput: CodeInputElement;
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

  codeLength: number;
  castButtonDisabled: boolean;

  constructor() {
    super();
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;

    this.codeLength = 6;
    this.castButtonDisabled = true;
  }

  ready() {
    super.ready();
    this.setState(PageState.CODE_INPUT);
    this.$.codeInput.addEventListener('access-code-input', (e: any) => {
      this.handleCodeInput(e);
    });
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

    if (state === PageState.CODE_INPUT) {
      this.$.codeInput.clearInput();
      this.$.codeInput.focusInput();
    }
  }

  private handleCodeInput(e: any) {
    this.castButtonDisabled = e.detail.value.length !== this.codeLength;
  }
}

customElements.define(AccessCodeCastElement.is, AccessCodeCastElement);
