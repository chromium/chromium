// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download list.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {ITEM_STATE} from './download_item.js';
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

    /** @private {!Array} */
    this.items_ = [];

    /** @private {!DownloadShelfApiProxy} */
    this.apiProxy_ = DownloadShelfApiProxyImpl.getInstance();

    /** @private {!Element} */
    this.listElement_ = assert(this.$('#downloadList'));

    /** @private {!Array<number>} */
    this.listenerIds_ = [];

    /** @private {ResizeObserver} */
    this.resizeObserver_ = new ResizeObserver(() => this.updateElements_());
    this.resizeObserver_.observe(this.listElement_);

    this.apiProxy_.getDownloads().then(downloadItems => {
      this.items_ = downloadItems;
      this.updateElements_();
    });

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    // TODO(romanarora): Once we implement a close panel button, we should
    // ensure it removes all listeners from this.listenerIds_ when triggered.

    // Triggers for downloads other than the first one, as the page handler will
    // not be ready by the first download.
    this.listenerIds_.push(callbackRouter.onNewDownload.addListener(
        (download_model) => {
            // TODO(romanarora): Implement this method as replacement to
            // onCreated.
        }));

    this.apiProxy_.onCreated(item => {
      this.items_.unshift(item);
      this.updateElements_();
    });

    this.apiProxy_.onChanged(changes => {
      const id = changes.id;
      const index = this.items_.findIndex(item => item.id === id);
      if (index >= 0) {
        const item = Object.assign({}, this.items_[index]);
        for (const key in changes) {
          if (key !== 'id') {
            item[key] = changes[key].current;
          }
        }
        this.items_[index] = item;
        this.updateElements_();
      }
    });

    this.apiProxy_.onErased(id => {
      const index = this.items_.findIndex(item => item.id === id);
      if (index >= 0) {
        this.items_.splice(index, 1);
        this.updateElements_();
      }
    });

    setInterval(() => this.onProgress_(), PROGRESS_UPDATE_INTERVAL);
  }

  /** @private */
  onProgress_() {
    for (const item of this.items_) {
      if (item.state !== ITEM_STATE.IN_PROGRESS || item.paused) {
        continue;
      }
      const downloadId = item.id;
      this.apiProxy_.getDownloadById(downloadId).then(items => {
        if (items.length > 0) {
          const index = this.items_.findIndex(item => item.id === downloadId);
          if (index >= 0) {
            this.items_[index] = items[0];
            this.updateElements_();
          }
        }
      });
    }
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
            downloadElement.removeEventListener('click', this.onItemClick_);
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
        downloadElement.addEventListener('click', this.onItemClick_);
        this.listElement_.appendChild(downloadElement);
        this.elements_.push(downloadElement);
      }
      currentWidth += downloadElement.offsetWidth;
    }

    /** Remove extra elements */
    if (itemCount < elementCount) {
      for (let i = itemCount; i < elementCount; ++i) {
        const downloadElement = this.elements_[i];
        downloadElement.removeEventListener('click', this.onItemClick_);
        this.listElement_.removeChild(downloadElement);
      }
      this.elements_.splice(itemCount, elementCount - itemCount);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    this.apiProxy_.showContextMenu(
        e.currentTarget.item.id, e.clientX, e.clientY);
  }
}

customElements.define('download-list', DownloadListElement);
