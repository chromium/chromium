// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';

import {Module, ModuleDescriptor} from './module_descriptor.js';
import {descriptors} from './module_descriptors.js';

/**
 * @fileoverview The module registry holds the descriptors of NTP modules and
 * provides management function such as instantiating the local module UIs.
 */

/** @type {ModuleRegistry} */
let instance = null;

export class ModuleRegistry {
  /** @return {!ModuleRegistry} */
  static getInstance() {
    return instance || (instance = new ModuleRegistry(descriptors));
  }

  /** @param {ModuleRegistry} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  /**
   * Creates a registry populated with a list of descriptors
   * @param {!Array<!ModuleDescriptor>} descriptors
   */
  constructor(descriptors) {
    /** @private {!Array<!ModuleDescriptor>} */
    this.descriptors_ = descriptors;
  }

  /** @return {!Array<!ModuleDescriptor>} */
  getDescriptors() {
    return this.descriptors_;
  }

  /**
   * Initializes enabled modules previously set via |registerModules| and
   * returns the initialized modules.
   * @param {number} timeout Timeout in milliseconds after which initialization
   *     of a particular module aborts.
   * @return {!Promise<!Array<!Module>>}
   */
  async initializeModules(timeout) {
    // Capture updateDisabledModules -> setDisabledModules round trip in a
    // promise for convenience.
    const disabledIds = await new Promise((resolve, _) => {
      const callbackRouter = NewTabPageProxy.getInstance().callbackRouter;
      const listenerId =
          callbackRouter.setDisabledModules.addListener((all, ids) => {
            callbackRouter.removeListener(listenerId);
            resolve(all ? this.descriptors_.map(({id}) => id) : ids);
          });
      NewTabPageProxy.getInstance().handler.updateDisabledModules();
    });
    const descriptors =
        this.descriptors_.filter(d => !disabledIds.includes(d.id));

    // Only conform to the persisted order if there exists one in the pref and
    // the feature is enabled. |orderedIds| will be an empty array if the user
    // has not reordered the modules before.
    const orderedIds = loadTimeData.getBoolean('modulesDragAndDropEnabled') ?
        (await NewTabPageProxy.getInstance().handler.getModulesOrder())
            .moduleIds :
        [];
    if (orderedIds.length > 0) {
      descriptors.sort((a, b) => {
        return orderedIds.indexOf(a.id) - orderedIds.indexOf(b.id);
      });
    }

    const elements =
        await Promise.all(descriptors.map(d => d.initialize(timeout)));
    return elements.map((e, i) => ({element: e, descriptor: descriptors[i]}))
        .filter(m => !!m.element);
  }
}
