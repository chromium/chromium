// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl, DataCollectorItem, UrlGenerationResult} from './browser_proxy.js';
import {getTemplate} from './url_generator.html.js';

export interface UrlGeneratorElement {
  $: {
    copyToast: CrToastElement,
    errorMessageToast: CrToastElement,
  };
}

export class UrlGeneratorElement extends PolymerElement {
  static get is() {
    return 'url-generator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      caseId_: {
        type: String,
        value: '',
      },
      dataCollectors_: {
        type: Array,
        value: () => [],
      },
      generatedURL_: {
        type: String,
        value: '',
      },
      urlGenerated_: {
        type: Boolean,
        value: false,
      },
      errorMessage_: {
        type: String,
        value: '',
      }
    };
  }

  private caseId_: string;
  private generatedURL_: string;
  private urlGenerated_: boolean;
  private errorMessage_: string;
  private dataCollectors_: DataCollectorItem[];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getAllDataCollectors().then(
        (dataCollectors: DataCollectorItem[]) => {
          this.dataCollectors_ = dataCollectors;
        });
  }

  private showErrorMessageToast_(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.$.errorMessageToast.show();
  }

  private showUrlGenerationResult_(result: UrlGenerationResult) {
    if (result.success) {
      this.generatedURL_ = result.url;
      this.urlGenerated_ = true;
    } else {
      this.showErrorMessageToast_(result.errorMessage);
    }
  }

  private onGenerateClick_() {
    this.browserProxy_.generateCustomizedURL(this.caseId_, this.dataCollectors_)
        .then(this.showUrlGenerationResult_.bind(this));
  }

  private onBackClick_() {
    this.generatedURL_ = '';
    this.urlGenerated_ = false;
  }

  private onCopyURLClick_() {
    navigator.clipboard.writeText(this.generatedURL_.toString());
    this.$.copyToast.show();
  }

  private onErrorMessageToastCloseClicked_() {
    this.$.errorMessageToast.hide();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'url-generator': UrlGeneratorElement;
  }
}

customElements.define(UrlGeneratorElement.is, UrlGeneratorElement);