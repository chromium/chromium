// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from '../browser_proxy.js';

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
   * @param {number} heightPx
   * @param {!InitializeModuleCallback} initializeCallback
   */
  constructor(id, heightPx, initializeCallback) {
    /** @private {string} */
    this.id_ = id;
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

  /** @return {number} */
  get heightPx() {
    return this.heightPx_;
  }

  /** @return {?HTMLElement} */
  get element() {
    return this.element_;
  }

  async initialize() {
    this.element_ = await this.initializeCallback_();
    if (!this.element_) {
      return;
    }
    BrowserProxy.getInstance().handler.onModuleLoaded(
        this.id_, BrowserProxy.getInstance().now());
  }
}
