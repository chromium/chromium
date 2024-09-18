// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './batch_upload_app.css.js';
import {getHtml} from './batch_upload_app.html.js';
import {BatchUploadBrowserProxyImpl} from './browser_proxy.js';
import type {BatchUploadBrowserProxy} from './browser_proxy.js';

export class BatchUploadAppElement extends CrLitElement {
  static get is() {
    return 'batch-upload-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      message_: {type: String},
    };
  }

  private batchUploadBrowserProxy_: BatchUploadBrowserProxy =
      BatchUploadBrowserProxyImpl.getInstance();
  protected message_: string = loadTimeData.getString('message');

  override connectedCallback() {
    super.connectedCallback();
    this.batchUploadBrowserProxy_.handler.updateViewHeight(300);
    this.batchUploadBrowserProxy_.callbackRouter.sendData.addListener(
        (data: string) => {
          this.message_ = data;
        });
  }

  protected close_() {
    this.batchUploadBrowserProxy_.handler.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'batch-upload-app': BatchUploadAppElement;
  }
}

customElements.define(BatchUploadAppElement.is, BatchUploadAppElement);
