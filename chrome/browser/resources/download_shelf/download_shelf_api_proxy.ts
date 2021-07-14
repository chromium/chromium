// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DownloadItem, PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './download_shelf.mojom-webui.js';

export interface DownloadShelfApiProxy {
  getCallbackRouter(): PageCallbackRouter;
  doShowAll(): void;
  doClose(): void;
  getDownloads(): Promise<{downloadItems: DownloadItem[]}>;
  getFileIcon(downloadId: number): Promise<string>;
  discardDownload(downloadId: number): void;
  keepDownload(downloadId: number): void;
  showContextMenu(
      downloadId: number, clientX: number, clientY: number,
      timestamp: number): void;
  openDownload(downloadId: number): void;
}

export class DownloadShelfApiProxyImpl implements DownloadShelfApiProxy {
  private callbackRouter: PageCallbackRouter;
  private handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }

  doShowAll(): void {
    this.handler.doShowAll();
  }

  doClose(): void {
    this.handler.doClose();
  }

  getDownloads(): Promise<{downloadItems: DownloadItem[]}> {
    return this.handler.getDownloads();
  }

  getFileIcon(downloadId: number): Promise<string> {
    return new Promise(resolve => {
      chrome.downloads.getFileIcon(downloadId, resolve);
    });
  }

  discardDownload(downloadId: number): void {
    this.handler.discardDownload(downloadId);
  }

  keepDownload(downloadId: number): void {
    this.handler.keepDownload(downloadId);
  }

  showContextMenu(
      downloadId: number, clientX: number, clientY: number,
      timestamp: number): void {
    this.handler.showContextMenu(downloadId, clientX, clientY, timestamp);
  }

  openDownload(downloadId: number): void {
    this.handler.openDownload(downloadId);
  }

  static getInstance(): DownloadShelfApiProxy {
    return instance || (instance = new DownloadShelfApiProxyImpl());
  }

  static setInstance(obj: DownloadShelfApiProxy) {
    instance = obj;
  }
}

let instance: DownloadShelfApiProxy|null = null;
