// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './code_input/code_input.js';
import './error_message/error_message.js';

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


import {AddSinkResultCode, CastDiscoveryMethod, PageCallbackRouter} from './access_code_cast.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import {CodeInputElement} from './code_input/code_input.js';
import {ErrorMessageElement} from './error_message/error_message.js';
import {RouteRequestResultCode} from './route_request_result_code.mojom-webui.js';

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
    dialog: CrDialogElement;
    errorMessage: ErrorMessageElement;
    qrInputView: HTMLDivElement;
  }
}

const AccessCodeCastElementBase = WebUIListenerMixin(I18nMixin(PolymerElement));

class AccessCodeCastElement extends AccessCodeCastElementBase {
  static get is() {
    return 'access-code-cast-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  private listenerIds: Array<number>;
  private router: PageCallbackRouter;

  private static readonly ACCESS_CODE_LENGTH = 6;
  private accessCode: string;
  private canCast: boolean;
  private state: PageState;
  private qrScannerEnabled: boolean;

  constructor() {
    super();
    this.listenerIds = [];
    this.router = BrowserProxy.getInstance().callbackRouter;
    this.canCast = true;

    this.accessCode = '';
    BrowserProxy.getInstance().isQrScanningAvailable().then((available) => {
      this.qrScannerEnabled = available;
    });

    window.onblur = () => {
      this.close();
    };

    document.addEventListener('keydown', (e: KeyboardEvent) => {
      if (e.key === 'Enter') {
        this.handleEnterPressed();
      }
    });
  }

  ready() {
    super.ready();
    this.setState(PageState.CODE_INPUT);
    this.$.errorMessage.setNoError();
    this.$.codeInput.addEventListener('access-code-input', (e: any) => {
      this.handleCodeInput(e);
    });
    this.$.dialog.showModal();
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(id => this.router.removeListener(id));
  }

  close() {
    BrowserProxy.getInstance().closeDialog();
  }

  switchToCodeInput() {
    this.setState(PageState.CODE_INPUT);
  }

  switchToQrInput() {
    this.setState(PageState.QR_INPUT);
  }

  async addSinkAndCast() {
    if (this.accessCode.length !== AccessCodeCastElement.ACCESS_CODE_LENGTH) {
      return;
    }
    if (!this.canCast) {
      return;
    }

    this.canCast = false;
    this.$.errorMessage.setNoError();

    const method = this.state === PageState.CODE_INPUT ? 
      CastDiscoveryMethod.INPUT_ACCESS_CODE : CastDiscoveryMethod.QR_CODE;

    const addResult = await this.addSink(method).catch(() => {
      return AddSinkResultCode.UNKNOWN_ERROR;
    });

    if (addResult !== AddSinkResultCode.OK) {
      this.$.errorMessage.setAddSinkError(addResult);
      this.canCast = true;
      return;
    }

    const castResult = await this.cast().catch(() => {
      return RouteRequestResultCode.UNKNOWN_ERROR;
    });

    if (castResult !== RouteRequestResultCode.OK) {
      this.$.errorMessage.setCastError(castResult);
      this.canCast = true;
      return;
    }

    this.close();
  }

  // Even though we can get this.accessCode directly, passing it triggers
  // Polymer's data binding whenever this.accessCode updates.
  castButtonDisabled(accessCode: string, canCast: boolean) {
    if (!canCast) {
      return true;
    }

    return accessCode.length !== AccessCodeCastElement.ACCESS_CODE_LENGTH;
  }

  setAccessCodeForTest(value: string) {
    this.accessCode = value;
  }

  private setState(state: PageState) {
    this.state = state;
    this.$.errorMessage.setNoError();

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
    this.accessCode = e.detail.value;
  }

  private handleEnterPressed() {
    if (this.castButtonDisabled(this.accessCode, this.canCast)) {
      return;
    }
    if (this.$.codeInput.getFocusedIndex() === -1) {
      return;
    }
    if (this.state !== PageState.CODE_INPUT) {
      return;
    }

    this.addSinkAndCast();
  }

  private async addSink(method: CastDiscoveryMethod):
      Promise<AddSinkResultCode> {
    const addSinkResult = await BrowserProxy.getInstance().handler
        .addSink(this.accessCode, method);
    return addSinkResult.resultCode as AddSinkResultCode;
  }

  private async cast(): Promise<RouteRequestResultCode> {
    const castResult = await BrowserProxy.getInstance().handler.castToSink();
    return castResult.resultCode as RouteRequestResultCode;
  }
}

customElements.define(AccessCodeCastElement.is, AccessCodeCastElement);
