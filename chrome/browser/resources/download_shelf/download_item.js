// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download item.
 */

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DownloadItem, DownloadState} from './download_shelf.mojom-webui.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

export class DownloadItemElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {DownloadItem} */
    this.item_;

    /** @private {!DownloadShelfApiProxy} */
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();
  }

  /** @param {!DownloadItem} value */
  set item(value) {
    if (this.item_ === value) {
      return;
    }
    this.item_ = value;
    this.update_();
  }

  /** @return {DownloadItem} */
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
    // Convert the value to a string as it might be a uint16 array for some
    // platforms.
    const filePath = String(item.fileNameToReportUser.path);
    this.$('#filename').innerText =
        filePath.substring(filePath.lastIndexOf('/') + 1);

    const statusTextElement = this.$('#statusText');
    const statusText = (!item.shouldPromoteOrigin || !item.originalUrl.url) ?
        item.statusText :
        new URL(item.originalUrl.url).origin;
    statusTextElement.innerText = statusText;

    downloadElement.dataset.state = item.state;
    switch (item.state) {
      case DownloadState.kInProgress:
        this.progress = item.totalBytes > 0 ?
            Number(item.receivedBytes) / Number(item.totalBytes) :
            0;
        break;
      case DownloadState.kComplete:
        this.progress = 1;
        break;
      case DownloadState.kInterrupted:
        this.progress = 0;
        break;
    }

    if (item.isPaused) {
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
