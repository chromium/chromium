// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
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
      errorMessage_: {
        type: String,
        value: '',
      },
      buttonDisabled_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private caseId_: string;
  private generatedURL_: string;
  private errorMessage_: string;
  private buttonDisabled_: boolean;
  private dataCollectors_: DataCollectorItem[];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getAllDataCollectors().then(
        (dataCollectors: DataCollectorItem[]) => {
          this.dataCollectors_ = dataCollectors;
        });
  }

  private onDataCollectorItemChange_() {
    // The button should be disabled if no data collector is selected.
    this.buttonDisabled_ = !this.hasDataCollectorSelected();
  }

  private hasDataCollectorSelected(): boolean {
    for (let index = 0; index < this.dataCollectors_.length; index++) {
      if (this.dataCollectors_[index]!.isIncluded) {
        return true;
      }
    }
    return false;
  }

  private showErrorMessageToast_(errorMessage: string) {
    this.errorMessage_ = errorMessage;
    this.$.errorMessageToast.show();
  }

  private onUrlGenerationResult_(result: UrlGenerationResult) {
    if (result.success) {
      this.generatedURL_ = result.url;
      navigator.clipboard.writeText(this.generatedURL_.toString());
      this.$.copyToast.show();
      this.$.copyToast.focus();
    } else {
      this.showErrorMessageToast_(result.errorMessage);
    }
  }

  private onCopyUrlClick_() {
    this.browserProxy_.generateCustomizedUrl(this.caseId_, this.dataCollectors_)
        .then(this.onUrlGenerationResult_.bind(this));
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
