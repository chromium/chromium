// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {DownloadItem, PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './download_shelf.mojom-webui.js';

/** @interface */
export class DownloadShelfApiProxy {
  /** @return {!PageCallbackRouter} */
  getCallbackRouter() {}

  doShowAll() {}

  doClose() {}

  /**
   * @return {!Promise<{
        downloadItems: !Array<!DownloadItem>,
   *  }>}
   */
  getDownloads() {}

  /**
   * @param {number} downloadId
   * @return {!Promise}
   */
  getFileIcon(downloadId) {}

  /** @param {number} downloadId */
  discardDownload(downloadId) {}

  /** @param {number} downloadId */
  keepDownload(downloadId) {}

  /**
   * @param {number} downloadId
   * @param {number} clientX
   * @param {number} clientY
   * @param {number} timestamp
   */
  showContextMenu(downloadId, clientX, clientY, timestamp) {}

  /**
   * @param {number} downloadId
   */
  openDownload(downloadId) {}
}

/** @implements {DownloadShelfApiProxy} */
export class DownloadShelfApiProxyImpl {
  constructor() {
    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageHandlerRemote} */
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** @override */
  doShowAll() {
    this.handler.doShowAll();
  }

  /** @override */
  doClose() {
    this.handler.doClose();
  }

  /** @override */
  getDownloads() {
    return this.handler.getDownloads();
  }

  /** @override */
  getFileIcon(downloadId) {
    return new Promise(resolve => {
      chrome.downloads.getFileIcon(downloadId, resolve);
    });
  }

  /** @override */
  discardDownload(downloadId) {
    this.handler.discardDownload(downloadId);
  }

  /** @override */
  keepDownload(downloadId) {
    this.handler.keepDownload(downloadId);
  }

  /** @override */
  showContextMenu(downloadId, clientX, clientY, timestamp) {
    this.handler.showContextMenu(downloadId, clientX, clientY, timestamp);
  }

  /** @override */
  openDownload(downloadId) {
    this.handler.openDownload(downloadId);
  }
}

addSingletonGetter(DownloadShelfApiProxyImpl);
