// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-lite.js';

import './read_later.mojom-lite.js';

/** @type {?ReadLaterApiProxy} */
let instance = null;

/** @interface */
export class ReadLaterApiProxy {
  /**
   * @return {!Promise<!{entries: !readLater.mojom.ReadLaterEntriesByStatus}>}
   */
  getReadLaterEntries() {}

  /**
   * @param {!url.mojom.Url} url
   * @param {boolean} mark_as_read
   * @param {!ui.mojom.ClickModifiers} click_modifiers
   */
  openURL(url, mark_as_read, click_modifiers) {}

  /**
   * @param {!url.mojom.Url} url
   * @param {boolean} read
   */
  updateReadStatus(url, read) {}

  addCurrentTab() {}

  /** @param {!url.mojom.Url} url */
  removeEntry(url) {}

  /**
   * @param {!url.mojom.Url} url
   * @param {number} locationX
   * @param {number} locationY
   */
  showContextMenuForURL(url, locationX, locationY) {}

  updateCurrentPageActionButtonState() {}

  showUI() {}

  closeUI() {}

  /** @return {!readLater.mojom.PageCallbackRouter} */
  getCallbackRouter() {}
}

/** @implements {ReadLaterApiProxy} */
export class ReadLaterApiProxyImpl {
  constructor() {
    /** @type {!readLater.mojom.PageCallbackRouter} */
    this.callbackRouter = new readLater.mojom.PageCallbackRouter();

    /** @type {!readLater.mojom.PageHandlerRemote} */
    this.handler = new readLater.mojom.PageHandlerRemote();

    const factory = readLater.mojom.PageHandlerFactory.getRemote();
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

