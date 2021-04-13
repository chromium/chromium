// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDuration, recordLoadDuration} from '../metrics_utils.js';
import {WindowProxy} from '../window_proxy.js';

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
   * @param {!InitializeModuleCallback} initializeCallback
   */
  constructor(id, name, initializeCallback) {
    /** @private {string} */
    this.id_ = id;
    /** @private {string} */
    this.name_ = name;
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
    const loadStartTime = WindowProxy.getInstance().now();
    this.element_ = await Promise.race([
      this.initializeCallback_(), new Promise(resolve => {
        WindowProxy.getInstance().setTimeout(() => {
          resolve(null);
        }, timeout);
      })
    ]);
    if (!this.element_) {
      return;
    }
    const loadEndTime = WindowProxy.getInstance().now();
    const duration = loadEndTime - loadStartTime;
    recordLoadDuration('NewTabPage.Modules.Loaded', loadEndTime);
    recordLoadDuration(`NewTabPage.Modules.Loaded.${this.id_}`, loadEndTime);
    recordDuration('NewTabPage.Modules.LoadDuration', duration);
    recordDuration(`NewTabPage.Modules.LoadDuration.${this.id_}`, duration);
  }
}
