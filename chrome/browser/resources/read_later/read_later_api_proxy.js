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

  /** @param {!url.mojom.Url} url */
  openSavedEntry(url) {}

  /**
   * @param {!url.mojom.Url} url
   * @param {boolean} read
   */
  updateReadStatus(url, read) {}

  /** @param {!url.mojom.Url} url */
  removeEntry(url) {}

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
  openSavedEntry(url) {
    this.handler.openSavedEntry(url);
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
  getCallbackRouter() {
    return this.callbackRouter;
  }
}

addSingletonGetter(ReadLaterApiProxyImpl);
