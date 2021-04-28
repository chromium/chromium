// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download list.
 */

import './download_item.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DownloadItem, DownloadState} from './download_shelf.mojom-webui.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from './download_shelf_api_proxy.js';

/** @type {number} */
const PROGRESS_UPDATE_INTERVAL = 500;

export class DownloadListElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!Array} */
    this.elements_ = [];

    /** @private {!Array<!DownloadItem>} */
    this.items_ = [];

    /** @private {!DownloadShelfApiProxy} */
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();

    /** @private {!Element} */
    this.listElement_ = assert(this.$('#download-list'));

    /** @private {!Array<number>} */
    this.listenerIds_ = [];

    /** @private {ResizeObserver} */
    this.resizeObserver_ = new ResizeObserver(() => this.updateElements_());
    this.resizeObserver_.observe(this.listElement_);

    this.apiProxy_.getDownloads().then(({downloadItems}) => {
      this.items_ = downloadItems;
      this.updateElements_();
    });

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    // TODO(romanarora): Once we implement a close panel button, we should
    // ensure it removes all listeners from this.listenerIds_ when triggered.

    // Triggers for downloads other than the first one, as the page handler will
    // not be ready by the first download.
    this.listenerIds_.push(
        callbackRouter.onNewDownload.addListener((downloadItem) => {
          this.items_.unshift(downloadItem);
          this.updateElements_();
        }));

    this.listenerIds_.push(
        callbackRouter.onDownloadUpdated.addListener((downloadItem) => {
          const index =
              this.items_.findIndex(item => item.id === downloadItem.id);
          if (index >= 0) {
            this.items_[index] = downloadItem;
            this.updateElements_();
          }
        }));

    this.listenerIds_.push(
        callbackRouter.onDownloadErased.addListener((downloadId) => {
          const index = this.items_.findIndex(item => item.id === downloadId);
          if (index >= 0) {
            this.items_.splice(index, 1);
            this.updateElements_();
          }
        }));
  }

  /** @private */
  updateElements_() {
    const containerWidth = this.listElement_.offsetWidth;
    let currentWidth = 0;
    const itemCount = this.items_.length;
    const elementCount = this.elements_.length;
    for (let i = 0; i < itemCount; ++i) {
      if (currentWidth >= containerWidth) {
        if (i < elementCount) /** Remove elements out of viewport. */ {
          for (let j = i; j < elementCount; ++j) {
            const downloadElement = this.elements_[j];
            this.listElement_.removeChild(downloadElement);
          }
          this.elements_.splice(i, elementCount - i);
        }
        break;
      }
      let downloadElement;
      if (i < elementCount) /** Update existing elements inside viewport. */ {
        downloadElement = this.elements_[i];
        downloadElement.item = this.items_[i];
      } else /** Insert new elements inside viewport */ {
        downloadElement = document.createElement('download-item');
        downloadElement.item = this.items_[i];
        this.listElement_.appendChild(downloadElement);
        this.elements_.push(downloadElement);
      }
      currentWidth += downloadElement.offsetWidth;
    }

    /** Remove extra elements */
    if (itemCount < elementCount) {
      for (let i = itemCount; i < elementCount; ++i) {
        const downloadElement = this.elements_[i];
        this.listElement_.removeChild(downloadElement);
      }
      this.elements_.splice(itemCount, elementCount - itemCount);
    }
  }
}

customElements.define('download-list', DownloadListElement);
