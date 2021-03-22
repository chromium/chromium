// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?WindowProxy} */
let instance = null;

/** Abstracts some builtin JS functions to mock them in tests. */
export class WindowProxy {
  /** @return {!WindowProxy} */
  static getInstance() {
    return instance || (instance = new WindowProxy());
  }

  /** @param {?WindowProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
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

  /** @param {?number} id */
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
