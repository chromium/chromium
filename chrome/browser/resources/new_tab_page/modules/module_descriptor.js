// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from '../browser_proxy.js';

/**
 * @fileoverview Provides the module descriptor. Each module must create a
 * module descriptor and register it at the NTP.
 */

/**
 * @typedef {{
 *   info: (function()|undefined),
 *   dismiss: (function():string|undefined),
 *   restore: (function()|undefined),
 * }}
 */
let Actions;

/**
 * @typedef {function(): !Promise<?{
 *    element: !HTMLElement,
 *    title: string,
 *    actions: (undefined|Actions),
 *  }>}
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
    /** @private {?string} */
    this.title_ = null;
    /** @private {HTMLElement} */
    this.element_ = null;
    /** @private {!InitializeModuleCallback} */
    this.initializeCallback_ = initializeCallback;
    /** @private {?Actions} */
    this.actions_ = null;
  }

  /** @return {string} */
  get id() {
    return this.id_;
  }

  /** @return {number} */
  get heightPx() {
    return this.heightPx_;
  }

  /** @return {?string} */
  get title() {
    return this.title_;
  }

  /** @return {?HTMLElement} */
  get element() {
    return this.element_;
  }

  /** @return {?Actions} */
  get actions() {
    return this.actions_;
  }

  async initialize() {
    const info = await this.initializeCallback_();
    if (!info) {
      return;
    }
    this.title_ = info.title;
    this.element_ = info.element;
    this.actions_ = info.actions || null;
    BrowserProxy.getInstance().handler.onModuleLoaded(
        this.id_, BrowserProxy.getInstance().now());
  }
}
