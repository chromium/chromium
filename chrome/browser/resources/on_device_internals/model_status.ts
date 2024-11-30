// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './model_status.css.js';
import {getHtml} from './model_status.html.js';

export class PageData {
  baseModelReady: boolean;

  constructor(baseModelReady: boolean) {
    this.baseModelReady = baseModelReady;
  }
}

export class OnDeviceInternalsModelStatusElement extends CrLitElement {
  constructor() {
    super();
    this.getOnDeviceInternalsData_();
  }

  static get is() {
    return 'on-device-internals-model-status';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageData_: {type: PageData},
    };
  }

  protected pageData_: PageData = {
    baseModelReady: false,
  };

  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  private async getOnDeviceInternalsData_() {
    this.pageData_.baseModelReady =
        (await this.proxy_.handler.getOnDeviceInternalsData())
            .pageData.baseModelReady;
    this.requestUpdate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-model-status': OnDeviceInternalsModelStatusElement;
  }
}

customElements.define(
    OnDeviceInternalsModelStatusElement.is,
    OnDeviceInternalsModelStatusElement);
