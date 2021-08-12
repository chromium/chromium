// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './whats_new_error_page.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WhatsNewCommandProxy} from './whats_new_command_proxy.js';
import {WhatsNewProxyImpl} from './whats_new_proxy.js';

export class WhatsNewAppElement extends PolymerElement {
  static get is() {
    return 'whats-new-app';
  }

  static get properties() {
    return {
      showErrorPage_: Boolean,
      url_: String,
    };
  }

  private showErrorPage_: boolean = false;
  private url_: string = '';

  connectedCallback() {
    super.connectedCallback();

    const queryParams = new URLSearchParams(window.location.search);
    const isAutoOpen = queryParams.has('auto');
    WhatsNewProxyImpl.getInstance().initialize(isAutoOpen).then(url => {
      if (!url) {
        this.showErrorPage_ = true;
        return;
      }

      this.url_ = isAutoOpen ? url.concat('?latest=true') : url;
    });
  }

  static get template() {
    return html`{__html_template__}`;
  }
}
customElements.define(WhatsNewAppElement.is, WhatsNewAppElement);
