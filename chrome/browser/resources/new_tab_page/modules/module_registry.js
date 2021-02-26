// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {BrowserProxy} from '../browser_proxy.js';
import {ModuleDescriptor} from './module_descriptor.js';

/**
 * @fileoverview The module registry holds the descriptors of NTP modules and
 * provides management function such as instantiating the local module UIs.
 */

export class ModuleRegistry {
  constructor() {
    /** @private {!Array<!ModuleDescriptor>} */
    this.descriptors_ = [];
  }

  /** @return {!Array<!ModuleDescriptor>} */
  getDescriptors() {
    return this.descriptors_;
  }

  /**
   * Registers modules via their descriptors.
   * @param {!Array<!ModuleDescriptor>} descriptors
   */
  registerModules(descriptors) {
    /** @type {!Array<!ModuleDescriptor>} */
    this.descriptors_ = descriptors;
  }

  /**
   * Initializes enabled modules previously set via |registerModules| and
   * returns the initialized descriptors.
   * @param {number} timeout Timeout in milliseconds after which initialization
   *     of a particular module aborts.
   * @return {!Promise<!Array<!ModuleDescriptor>>}
   */
  async initializeModules(timeout) {
    // Capture updateDisabledModules -> setDisabledModules round trip in a
    // promise for convenience.
    const disabledIds = await new Promise((resolve, _) => {
      const callbackRouter = BrowserProxy.getInstance().callbackRouter;
      const listenerId =
          callbackRouter.setDisabledModules.addListener((all, ids) => {
            callbackRouter.removeListener(listenerId);
            resolve(all ? this.descriptors_.map(({id}) => id) : ids);
          });
      BrowserProxy.getInstance().handler.updateDisabledModules();
    });
    await Promise.all(
        this.descriptors_.filter(d => disabledIds.indexOf(d.id) < 0)
            .map(d => d.initialize(timeout)));
    return this.descriptors_.filter(descriptor => !!descriptor.element);
  }
}

addSingletonGetter(ModuleRegistry);
