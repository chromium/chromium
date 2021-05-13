// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';

import './read_later.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class ReadLaterApiProxy {
  /**
   * @return {!Promise<!{entries: !readLater.mojom.ReadLaterEntriesByStatus}>}
   */
  getReadLaterEntries() {}

  /**
   * @param {!url.mojom.Url} url
   * @param {boolean} mark_as_read
   */
  openURL(url, mark_as_read) {}

  /**
   * @param {!url.mojom.Url} url
   * @param {boolean} read
   */
  updateReadStatus(url, read) {}

  /** @param {!url.mojom.Url} url */
  removeEntry(url) {}

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
  openURL(url, mark_as_read) {
    this.handler.openURL(url, mark_as_read);
  }

  /** @override */
  updateReadStatus(url, read) {
    this.handler.updateReadStatus(url, read);
  }

  /** @override */
  removeEntry(url) {
    this.handler.removeEntry(url);
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
}

addSingletonGetter(ReadLaterApiProxyImpl);
