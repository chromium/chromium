// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, ReadLaterEntriesByStatus} from './reading_list.mojom-webui.js';

let instance: ReadingListApiProxy|null = null;

export interface ReadingListApiProxy {
  getReadLaterEntries(): Promise<{entries: ReadLaterEntriesByStatus}>;

  openURL(url: Url, markAsRead: boolean, clickModifiers: ClickModifiers): void;

  updateReadStatus(url: Url, read: boolean): void;

  addCurrentTab(): void;

  removeEntry(url: Url): void;

  showContextMenuForURL(url: Url, locationX: number, locationY: number): void;

  updateCurrentPageActionButtonState(): void;

  showUI(): void;

  closeUI(): void;

  getCallbackRouter(): PageCallbackRouter;
}

export class ReadingListApiProxyImpl implements ReadingListApiProxy {
  private callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  private handler: PageHandlerRemote = new PageHandlerRemote();

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getReadLaterEntries() {
    return this.handler.getReadLaterEntries();
  }

  openURL(url: Url, markAsRead: boolean, clickModifiers: ClickModifiers) {
    this.handler.openURL(url, markAsRead, clickModifiers);
  }

  updateReadStatus(url: Url, read: boolean) {
    this.handler.updateReadStatus(url, read);
  }

  addCurrentTab() {
    this.handler.addCurrentTab();
  }

  removeEntry(url: Url) {
    this.handler.removeEntry(url);
  }

  showContextMenuForURL(url: Url, locationX: number, locationY: number) {
    this.handler.showContextMenuForURL(url, locationX, locationY);
  }

  updateCurrentPageActionButtonState() {
    this.handler.updateCurrentPageActionButtonState();
  }

  showUI() {
    this.handler.showUI();
  }

  closeUI() {
    this.handler.closeUI();
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): ReadingListApiProxy {
    return instance || (instance = new ReadingListApiProxyImpl());
  }

  static setInstance(obj: ReadingListApiProxy) {
    instance = obj;
  }
}
