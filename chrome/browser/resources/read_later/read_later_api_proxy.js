// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote, ReadLaterEntriesByStatus} from './read_later.mojom-webui.js';

/** @type {?ReadLaterApiProxy} */
let instance = null;

/** @interface */
export class ReadLaterApiProxy {
  /**
   * @return {!Promise<!{entries: !ReadLaterEntriesByStatus}>}
   */
  getReadLaterEntries() {}

  /**
   * @param {!Url} url
   * @param {boolean} mark_as_read
   * @param {!ClickModifiers} click_modifiers
   */
  openURL(url, mark_as_read, click_modifiers) {}

  /**
   * @param {!Url} url
   * @param {boolean} read
   */
  updateReadStatus(url, read) {}

  addCurrentTab() {}

  /** @param {!Url} url */
  removeEntry(url) {}

  /**
   * @param {!Url} url
   * @param {number} locationX
   * @param {number} locationY
   */
  showContextMenuForURL(url, locationX, locationY) {}

  updateCurrentPageActionButtonState() {}

  showUI() {}

  closeUI() {}

  /** @return {!PageCallbackRouter} */
  getCallbackRouter() {}
}

/** @implements {ReadLaterApiProxy} */
export class ReadLaterApiProxyImpl {
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
  getReadLaterEntries() {
    return this.handler.getReadLaterEntries();
  }

  /** @override */
  openURL(url, mark_as_read, click_info) {
    this.handler.openURL(url, mark_as_read, click_info);
  }

  /** @override */
  updateReadStatus(url, read) {
    this.handler.updateReadStatus(url, read);
  }

  /** @override */
  addCurrentTab() {
    this.handler.addCurrentTab();
  }

  /** @override */
  removeEntry(url) {
    this.handler.removeEntry(url);
  }

  /** @override */
  showContextMenuForURL(url, locationX, locationY) {
    this.handler.showContextMenuForURL(url, locationX, locationY);
  }

  /** @override */
  updateCurrentPageActionButtonState() {
    this.handler.updateCurrentPageActionButtonState();
  }

  /** @override */
  showUI() {
    this.handler.showUI();
  }

  /** @override */
  closeUI() {
    this.handler.closeUI();
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** @return {!ReadLaterApiProxy} */
  static getInstance() {
    return instance || (instance = new ReadLaterApiProxyImpl());
  }

  /** @param {!ReadLaterApiProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

