// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './download_list.js';
import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

export class DownloadShelfAppElement extends CustomElement {
  static override get template() {
    return `{__html_template__}`;
  }

  private apiProxy_: DownloadShelfApiProxy;

  constructor() {
    super();

    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();

    const showAllButton = this.$<HTMLElement>('#show-all-button')!;
    showAllButton.innerText = loadTimeData.getString('showAll');
    showAllButton.addEventListener('click', () => this.onShowAll_());

    const closeButton = this.$('#close-button')!;
    closeButton.setAttribute('aria-label', loadTimeData.getString('close'));
    closeButton.addEventListener('click', () => this.onClose_());
  }

  private onShowAll_() {
    this.apiProxy_.doShowAll();
  }

  private onClose_() {
    this.apiProxy_.doClose();
  }
}

customElements.define('download-shelf-app', DownloadShelfAppElement);
