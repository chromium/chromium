// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './model_status.css.js';
import {getHtml} from './model_status.html.js';
import type {OnDeviceInternalsData} from './on_device_internals_page.mojom-webui.js';

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

  static override get properties() {
    return {
      pageData_: {type: Object},
      mayRestartBrowser_: {type: Boolean},
    };
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected pageData_: OnDeviceInternalsData = {
    baseModelReady: false,
    modelState: 'NO STATE',
    registrationCriteria: {},
    suppModels: [],
    modelCrashCount: 0,
    maxModelCrashCount: 0,
  };

  protected mayRestartBrowser_: boolean = false;
  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  protected async onResetModelCrashCountClick_() {
    await this.proxy_.handler.resetModelCrashCount();
    await this.getOnDeviceInternalsData_();
    this.mayRestartBrowser_ = true;
  }

  private async getOnDeviceInternalsData_() {
    this.pageData_ =
        (await this.proxy_.handler.getOnDeviceInternalsData()).pageData;
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
