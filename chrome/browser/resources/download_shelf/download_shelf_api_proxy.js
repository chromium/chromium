// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './download_shelf.mojom-webui.js';

/** @interface */
export class DownloadShelfApiProxy {
  /** @return {!PageCallbackRouter} */
  getCallbackRouter() {}

  /**
   * @return {!Promise<!Array<!chrome.downloads.DownloadItem>>}
   */
  getDownloads() {}

  /**
   * @param {number} downloadId
   * @return {!Promise}
   */
  getDownloadById(downloadId) {}

  /**
   * @param {number} downloadId
   * @return {!Promise}
   */
  getFileIcon(downloadId) {}

  /**
   * @param {function(!Object)} callback
   */
  onCreated(callback) {}

  /**
   * @param {function(!Object)} callback
   */
  onChanged(callback) {}

  /**
   * @param {function(number)} callback
   */
  onErased(callback) {}

  /**
   * @param {number} downloadId
   * @param {number} clientX
   * @param {number} clientY
   */
  showContextMenu(downloadId, clientX, clientY) {}
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
  getDownloads() {
    return new Promise(resolve => {
      chrome.downloads.search(
          {
            orderBy: ['-startTime'],
            limit: 100,
          },
          resolve);
    });
  }

  /** @override */
  getDownloadById(downloadId) {
    return new Promise(resolve => {
      chrome.downloads.search(
          {
            id: downloadId,
          },
          resolve);
    });
  }

  /** @override */
  getFileIcon(downloadId) {
    return new Promise(resolve => {
      chrome.downloads.getFileIcon(downloadId, resolve);
    });
  }

  /** @override */
  onCreated(callback) {
    chrome.downloads.onCreated.addListener(callback);
  }

  /** @override */
  onChanged(callback) {
    chrome.downloads.onChanged.addListener(callback);
  }

  /** @override */
  onErased(callback) {
    chrome.downloads.onErased.addListener(callback);
  }

  /** @override */
  showContextMenu(downloadId, clientX, clientY) {
    this.handler.showContextMenu(downloadId, clientX, clientY);
  }
}

addSingletonGetter(DownloadShelfApiProxyImpl);
