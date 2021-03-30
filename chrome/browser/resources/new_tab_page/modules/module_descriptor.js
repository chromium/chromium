// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from '../browser_proxy.js';
import {mojoTimeDelta} from '../utils.js';

/**
 * @fileoverview Provides the module descriptor. Each module must create a
 * module descriptor and register it at the NTP.
 */

/**
 * @typedef {function(): !Promise<?HTMLElement>}
 */
let InitializeModuleCallback;

export class ModuleDescriptor {
  /**
   * @param {string} id
   * @param {string} name
   * @param {number} heightPx
   * @param {!InitializeModuleCallback} initializeCallback
   */
  constructor(id, name, heightPx, initializeCallback) {
    /** @private {string} */
    this.id_ = id;
    /** @private {string} */
    this.name_ = name;
    /** @private {number} */
    this.heightPx_ = heightPx;
    /** @private {HTMLElement} */
    this.element_ = null;
    /** @private {!InitializeModuleCallback} */
    this.initializeCallback_ = initializeCallback;
  }

  /** @return {string} */
  get id() {
    return this.id_;
  }

  /** @return {string} */
  get name() {
    return this.name_;
  }

  /** @return {number} */
  get heightPx() {
    return this.heightPx_;
  }

  /** @return {?HTMLElement} */
  get element() {
    return this.element_;
  }

  /**
   * Initializes the module. On success, |this.element| will be populated after
   * the returned promise has resolved.
   * @param {number} timeout Timeout in milliseconds after which initialization
   *     aborts.
   * @return {!Promise}
   */
  async initialize(timeout) {
    const loadStartTime = BrowserProxy.getInstance().now();
    this.element_ = await Promise.race([
      this.initializeCallback_(), new Promise(resolve => {
        BrowserProxy.getInstance().setTimeout(() => {
          resolve(null);
        }, timeout);
      })
    ]);
    if (!this.element_) {
      return;
    }
    if (this.element_.height !== undefined) {
      this.heightPx_ = this.element_.height;
    }
    const loadEndTime = BrowserProxy.getInstance().now();
    BrowserProxy.getInstance().handler.onModuleLoaded(
        this.id_, mojoTimeDelta(loadEndTime - loadStartTime), loadEndTime);
  }
}
