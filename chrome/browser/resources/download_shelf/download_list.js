// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview UI element of a download list.
 */

import './download_item.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DownloadItem} from './download_shelf.mojom-webui.js';
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

    this.getDownloads_(true);
    this.addDownloadListeners_();

    document.addEventListener('visibilitychange', () => {
      if (document.visibilityState === 'visible') {
        this.getDownloads_(false);
        this.addDownloadListeners_();
      } else {
        this.clear_();
      }
    });
  }

  /** @private */
  addDownloadListeners_() {
    const callbackRouter = this.apiProxy_.getCallbackRouter();

    // Triggers for downloads other than the first one, as the page handler will
    // not be ready by the first download.
    this.listenerIds_.push(
        callbackRouter.onNewDownload.addListener((downloadItem) => {
          this.items_.unshift(downloadItem);
          this.updateElements_();
          this.recordDownloadPaintTime_(
              downloadItem.showDownloadStartTime, false);
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

  /**
   * @param {boolean} firstCall Whether this is the first call to the method.
   * @private
   */
  getDownloads_(firstCall) {
    this.apiProxy_.getDownloads().then(({downloadItems}) => {
      this.items_ = downloadItems;

      if (this.items_.length !== 0) {
        // Sort by descending show time so that the most recent download shows
        // first on the row.
        this.items_.sort(
            (first, second) =>
                second.showDownloadStartTime - first.showDownloadStartTime);
        this.updateElements_();

        // Record the time it took to show the first download, that is, the
        // download that has the least recent showDownloadStartTime.
        if (firstCall) {
          this.recordDownloadPaintTime_(
              this.items_[this.items_.length - 1].showDownloadStartTime, true);
        }
      }
    });
  }

  clear_() {
    while (this.listenerIds_.length) {
      this.apiProxy_.getCallbackRouter().removeListener(
          this.listenerIds_.shift());
    }

    while (this.listElement_.firstChild) {
      this.listElement_.removeChild(this.listElement_.firstChild);
    }

    this.elements_ = [];
    this.items_ = [];
  }

  /**
   * @param {number} startTime The Unix time at which DoShowDownload() was
   *     called on the download shelf. See:
   *     chrome/browser/ui/webui/download_shelf/download_shelf_ui.h
   * @param {boolean} isFirstDownload
   * @private
   */
  recordDownloadPaintTime_(startTime, isFirstDownload) {
    window.requestAnimationFrame(() => {
      window.requestAnimationFrame(() => {
        const elapsedTime = Math.round(Date.now() - startTime);
        chrome.metricsPrivate.recordTime(
            isFirstDownload ? 'Download.Shelf.WebUI.FirstDownloadPaintTime' :
                              'Download.Shelf.WebUI.NotFirstDownloadPaintTime',
            elapsedTime);
      });
    });
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
