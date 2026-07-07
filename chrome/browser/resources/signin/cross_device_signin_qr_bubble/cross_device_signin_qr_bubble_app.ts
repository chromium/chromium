// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';

import {PageHandlerFactory, PageHandlerRemote} from '../cross_device_signin_qr_bubble.mojom-webui.js';

import {getCss} from './cross_device_signin_qr_bubble_app.css.js';
import {getHtml} from './cross_device_signin_qr_bubble_app.html.js';

const CrossDeviceSigninQrBubbleAppElementBase = I18nMixinLit(CrLitElement);

export class CrossDeviceSigninQrBubbleAppElement extends
    CrossDeviceSigninQrBubbleAppElementBase {
  static get is() {
    return 'cross-device-signin-qr-bubble-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      fullName: {type: String},
      email: {type: String},
      qrCodeDataUri: {type: String},
    };
  }

  protected accessor fullName: string = '';
  protected accessor email: string = '';
  protected accessor qrCodeDataUri: string = '';

  private handler_: PageHandlerRemote = new PageHandlerRemote();

  override async connectedCallback() {
    super.connectedCallback();

    PageHandlerFactory.getRemote().createCrossDeviceSigninQrBubbleHandler(
        this.handler_.$.bindNewPipeAndPassReceiver());

    const response = await this.handler_.getRegistrationData();
    if (!response.data) {
      return;
    }
    const data = response.data;
    this.fullName = data.fullName;
    this.email = data.email;
    this.qrCodeDataUri = data.qrCodeDataUri;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cross-device-signin-qr-bubble-app': CrossDeviceSigninQrBubbleAppElement;
  }
}

customElements.define(
    CrossDeviceSigninQrBubbleAppElement.is,
    CrossDeviceSigninQrBubbleAppElement);
