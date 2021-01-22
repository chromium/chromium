// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';

import './realbox/omnibox.mojom-lite.js';
import './realbox/realbox.mojom-lite.js';
import './new_tab_page.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class BrowserProxy {
  constructor() {
    /** @type {newTabPage.mojom.PageCallbackRouter} */
    this.callbackRouter = new newTabPage.mojom.PageCallbackRouter();

    /** @type {newTabPage.mojom.PageHandlerRemote} */
    this.handler = new newTabPage.mojom.PageHandlerRemote();

    const factory = newTabPage.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  /** @param {string} href */
  navigate(href) {
    window.location.href = href;
  }

  /** @param {string} url */
  open(url) {
    window.open(url, '_blank');
  }

  /**
   * @param {function()} callback
   * @param {number} duration
   * @return {number}
   */
  setTimeout(callback, duration) {
    return window.setTimeout(callback, duration);
  }

  /** @param {number} id */
  clearTimeout(id) {
    window.clearTimeout(id);
  }

  /** @return {number} */
  random() {
    return Math.random();
  }

  /**
   * @param {string} src
   * @return {string}
   */
  createIframeSrc(src) {
    return src;
  }

  /**
   * @param {string} query
   * @return {!MediaQueryList}
   */
  matchMedia(query) {
    return window.matchMedia(query);
  }

  /** @return {number} */
  now() {
    return Date.now();
  }

  /**
   * Returns promise that resolves when lazy rendering should be started.
   * @return {!Promise}
   */
  waitForLazyRender() {
    return new Promise((resolve, reject) => {
      requestIdleCallback(resolve, {timeout: 500});
    });
  }

  /**
   * Posts |message| on the content window of |iframe| at |targetOrigin|.
   * @param {!HTMLIFrameElement} iframe
   * @param {*} message
   * @param {string} targetOrigin
   */
  postMessage(iframe, message, targetOrigin) {
    iframe.contentWindow.postMessage(message, targetOrigin);
  }
}

addSingletonGetter(BrowserProxy);
