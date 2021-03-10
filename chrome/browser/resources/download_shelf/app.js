// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './download_item.js';
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

    this.apiProxy_.getDownloads().then(downloadItems => {
      const downloadList = this.$('#downloadList');
      for (const item of downloadItems) {
        const downloadElement = document.createElement('download-item');
        downloadElement.item = item;
        downloadElement.addEventListener('click', this.onItemClick_);
        downloadList.appendChild(downloadElement);
      }
    });
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    // TODO: access download item from e.currentTarget.item.
  }
}

customElements.define('download-shelf-app', DownloadShelfAppElement);
