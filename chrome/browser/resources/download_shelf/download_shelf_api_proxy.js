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
   * @return {!Promise<!Array<!chrome.downloads.DownloadItem>>} callback
   */
  getDownloads() {}
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
}

addSingletonGetter(DownloadShelfApiProxyImpl);
