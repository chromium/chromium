// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './support_tool.html.js';

export class SupportToolElement extends PolymerElement {
  static get is() {
    return 'support-tool';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      caseId_: {
        type: String,
        value: () => loadTimeData.getString('caseId'),
      },
      emails_: {
        type: Array,
        value: () => [],
      },
      issueDescription_: {
        type: String,
        value: '',
      },
      selectedEmail_: {
        type: String,
        value: '',
      },
    };
  }

  private caseId_: string;
  private emails_: string[];
  private issueDescription_: string;
  private selectedEmail_: string;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.getEmailAddresses().then((emails: string[]) => {
      this.emails_ = emails;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'support-tool': SupportToolElement;
  }
}

customElements.define(SupportToolElement.is, SupportToolElement);