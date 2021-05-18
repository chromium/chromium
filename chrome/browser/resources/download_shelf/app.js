// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './download_list.js';
import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

export class DownloadShelfAppElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!DownloadShelfApiProxy} */
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();

    this.$('#close-button').addEventListener('click', e => this.onClose_(e));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onClose_(e) {
    this.apiProxy_.doClose();
  }
}

customElements.define('download-shelf-app', DownloadShelfAppElement);
