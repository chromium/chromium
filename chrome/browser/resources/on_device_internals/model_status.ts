// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './model_status.css.js';
import {getHtml} from './model_status.html.js';
import type {PageData} from './on_device_internals_page.mojom-webui.js';

export class OnDeviceInternalsModelStatusElement extends CrLitElement {
  constructor() {
    super();
    this.getPageData_();
  }

  static get is() {
    return 'on-device-internals-model-status';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      pageData_: {type: Object},
      mayRestartBrowser_: {type: Boolean},
    };
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected accessor pageData_: PageData = {
    baseModel: {
      state: 'NO STATE',
      registrationCriteria: {},
      info: null,
    },
    suppModels: [],
    modelCrashCount: 0,
    maxModelCrashCount: 0,
  };

  protected accessor mayRestartBrowser_: boolean = false;
  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  protected async onResetModelCrashCountClick_() {
    await this.proxy_.handler.resetModelCrashCount();
    await this.getPageData_();
    this.mayRestartBrowser_ = true;
  }

  private async getPageData_() {
    this.pageData_ = (await this.proxy_.handler.getPageData()).pageData;
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
