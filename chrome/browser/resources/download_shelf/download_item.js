// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download item.
 */

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

/** @enum {string} */
export const ITEM_STATE = {
  IN_PROGRESS: 'in_progress',
  INTERRUPTED: 'interrupted',
  COMPLETE: 'complete',
};

export class DownloadItemElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {chrome.downloads.DownloadItem} */
    this.item_;

    /** @private {!DownloadShelfApiProxy} */
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();
  }

  /** @param {chrome.downloads.DownloadItem} value  */
  set item(value) {
    if (this.item_ === value) {
      return;
    }
    this.item_ = value;
    this.update_();
  }

  /** @return {chrome.downloads.DownloadItem} */
  get item() {
    return this.item_;
  }

  /** @private */
  update_() {
    const item = this.item_;
    if (!item) {
      return;
    }
    const downloadElement = this.$('.download-item');
    this.$('#filename').innerText =
        item.filename.substring(item.filename.lastIndexOf('/') + 1);
    downloadElement.dataset.state = item.state;
    switch (item.state) {
      case ITEM_STATE.IN_PROGRESS:
        this.progress =
            item.totalBytes > 0 ? item.bytesReceived / item.totalBytes : 0;
        break;
      case ITEM_STATE.COMPLETE:
        this.progress = 1;
        break;
      case ITEM_STATE.INTERRUPTED:
        this.progress = 0;
        break;
    }

    if (item.paused) {
      downloadElement.dataset.paused = true;
    } else {
      delete downloadElement.dataset.paused;
    }

    this.apiProxy_.getFileIcon(item.id).then(icon => {
      this.$('#fileIcon').src = icon;
    });
  }

  /** @param {number} value */
  set progress(value) {
    this.$('.progress')
        .style.setProperty('--download-progress', value.toString());
  }
}

customElements.define('download-item', DownloadItemElement);
