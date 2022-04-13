// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './support_tool_shared_css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, BrowserProxyImpl, DataCollectorItem} from './browser_proxy.js';
import {getTemplate} from './url_generator.html.js';

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
      }
    };
  }

  private caseId_: string;
  private generatedURL_: string;
  private dataCollectors_: DataCollectorItem[];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getAllDataCollectors().then(
        (dataCollectors: DataCollectorItem[]) => {
          this.dataCollectors_ = dataCollectors;
        });
  }

  private onGenerateClick_() {
    // TODO(b/217931906): Send signal to generate URL through BrowserProxy and
    // make input fields disabled.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'url-generator': UrlGeneratorElement;
  }
}

customElements.define(UrlGeneratorElement.is, UrlGeneratorElement);