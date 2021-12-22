// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recordDuration, recordLoadDuration} from '../metrics_utils.js';
import {WindowProxy} from '../window_proxy.js';

/**
 * @fileoverview Provides the module descriptor. Each module must create a
 * module descriptor and register it at the NTP.
 */

/** @typedef {function(): !Promise<?HTMLElement>} */
export let InitializeModuleCallback;

/** @typedef {function(): !Promise<!HTMLElement>} */
export let InitializeModuleCallbackV2;

/** @typedef {{element: !HTMLElement, descriptor: !ModuleDescriptor}} */
export let Module;

/**
 * @enum {number}
 * @const
 */
export const ModuleHeight = {
  DYNAMIC: -1,
  SHORT: 166,
  TALL: 358,
};

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
  get height() {
    return ModuleHeight.DYNAMIC;
  }

  /**
   * Initializes the module and returns the module element on success.
   * @param {number} timeout Timeout in milliseconds after which initialization
   *     aborts.
   * @return {!Promise<?HTMLElement>}
   */
  async initialize(timeout) {
    const loadStartTime = WindowProxy.getInstance().now();
    const element = await Promise.race([
      this.initializeCallback_(), new Promise(resolve => {
        WindowProxy.getInstance().setTimeout(() => {
          resolve(null);
        }, timeout);
      })
    ]);
    if (!element) {
      return null;
    }
    const loadEndTime = WindowProxy.getInstance().now();
    const duration = loadEndTime - loadStartTime;
    recordLoadDuration('NewTabPage.Modules.Loaded', loadEndTime);
    recordLoadDuration(`NewTabPage.Modules.Loaded.${this.id_}`, loadEndTime);
    recordDuration('NewTabPage.Modules.LoadDuration', duration);
    recordDuration(`NewTabPage.Modules.LoadDuration.${this.id_}`, duration);
    return element;
  }
}

export class ModuleDescriptorV2 extends ModuleDescriptor {
  /**
   * @param {string} id
   * @param {string} name
   * @param {!ModuleHeight} height
   * @param {!InitializeModuleCallbackV2} initializeCallback
   */
  constructor(id, name, height, initializeCallback) {
    super(id, name, initializeCallback);
    /** @private {!ModuleHeight} */
    this.height_ = height;
  }

  /** @override */
  get height() {
    return this.height_;
  }

  /**
   * Like |ModuleDescriptor.initialize()| but returns an empty element on
   * timeout.
   * @param {number} timeout
   * @return {!Promise<!HTMLElement>}
   */
  async initialize(timeout) {
    return (await super.initialize(timeout)) ||
        /** @type {!HTMLElement} */ (document.createElement('div'));
  }
}
