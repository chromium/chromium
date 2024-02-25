// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import type {ReadLaterEntriesByStatus} from './reading_list.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './reading_list.mojom-webui.js';

let instance: ReadingListApiProxy|null = null;

export interface ReadingListApiProxy {
  getReadLaterEntries(): Promise<{entries: ReadLaterEntriesByStatus}>;

  openUrl(url: Url, markAsRead: boolean, clickModifiers: ClickModifiers): void;

  updateReadStatus(url: Url, read: boolean): void;

  markCurrentTabAsRead(): void;

  addCurrentTab(): void;

  removeEntry(url: Url): void;

  showContextMenuForUrl(url: Url, locationX: number, locationY: number): void;

  updateCurrentPageActionButtonState(): void;

  showUi(): void;

  closeUi(): void;

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

  openUrl(url: Url, markAsRead: boolean, clickModifiers: ClickModifiers) {
    this.handler.openURL(url, markAsRead, clickModifiers);
  }

  updateReadStatus(url: Url, read: boolean) {
    this.handler.updateReadStatus(url, read);
  }

  markCurrentTabAsRead() {
    this.handler.markCurrentTabAsRead();
  }

  addCurrentTab() {
    this.handler.addCurrentTab();
  }

  removeEntry(url: Url) {
    this.handler.removeEntry(url);
  }

  showContextMenuForUrl(url: Url, locationX: number, locationY: number) {
    this.handler.showContextMenuForURL(url, locationX, locationY);
  }

  updateCurrentPageActionButtonState() {
    this.handler.updateCurrentPageActionButtonState();
  }

  showUi() {
    this.handler.showUI();
  }

  closeUi() {
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
