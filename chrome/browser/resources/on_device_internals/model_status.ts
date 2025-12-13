// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './model_status.css.js';
import {getHtml} from './model_status.html.js';
import type {PageData} from './on_device_internals_page.mojom-webui.js';
import {PerformanceClass} from './on_device_model.mojom-webui.js';

export class OnDeviceInternalsModelStatusElement extends CrLitElement {
  constructor() {
    super();
    this.getPageData_();
  }

  private formatBytes(bytes: number) {
    if (bytes === 0) {
      return '0 Bytes';
    }

    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];

    const i =
        Math.min(Math.round(Math.log(bytes) / Math.log(k)) - 1, sizes.length);

    return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
  }

  override connectedCallback() {
    super.connectedCallback();
    BrowserProxy.getInstance()
        .callbackRouter.onDownloadProgressUpdate.addListener(
            this.logProgress_.bind(this));
  }

  private logProgress_(downloadedBytes: number, totalBytes: number) {
    this.loadProgress = Number(downloadedBytes);
    this.loadMax = Number(totalBytes);
    this.readableLoadProgress = this.formatBytes(this.loadProgress);
    this.readableLoadMax = this.formatBytes(this.loadMax);
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
      loadProgress: {type: Number},
      loadMax: {type: Number},
      readableLoadProgress: {type: String},
      readableLoadMax: {type: String},
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
    featureAdaptations: [],
    performanceInfo: {
      performanceClass: PerformanceClass.kError,
      vramMb: 0n,
    },
    minVramMb: 0n,
  };

  protected accessor mayRestartBrowser_: boolean = false;
  private proxy_: BrowserProxy = BrowserProxy.getInstance();

  protected accessor loadProgress: number = 0;
  protected accessor loadMax: number = 100;
  protected accessor readableLoadProgress: string = '_';
  protected accessor readableLoadMax: string = '_';

  protected async onResetModelCrashCountClick_() {
    await this.proxy_.handler.resetModelCrashCount();
    await this.getPageData_();
    this.mayRestartBrowser_ = true;
  }

  protected async onFeatureUsageSetterClick_(
      feature: number, isRecentlyUsed: boolean) {
    await this.proxy_.handler.setFeatureRecentlyUsedState(
        feature, isRecentlyUsed);
    this.getPageData_();
  }

  private async getPageData_() {
    this.pageData_ = (await this.proxy_.handler.getPageData()).pageData;
  }

  protected uninstallDefaultModel_() {
    this.proxy_.handler.uninstallDefaultModel();
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
