// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ModuleIdName} from '../new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';

import type {Module, ModuleDescriptor} from './module_descriptor.js';
import {descriptors} from './module_descriptors.js';

/**
 * @fileoverview The module registry holds the descriptors of NTP modules and
 * provides management function such as instantiating the local module UIs.
 */

let instance: ModuleRegistry|null = null;

export class ModuleRegistry {
  static getInstance(): ModuleRegistry {
    return instance || (instance = new ModuleRegistry(descriptors));
  }

  static setInstance(newInstance: ModuleRegistry) {
    instance = newInstance;
  }

  private descriptors_: ModuleDescriptor[];

  /** Creates a registry populated with a list of descriptors. */
  constructor(descriptors: ModuleDescriptor[]) {
    this.descriptors_ = descriptors;
  }

  /**
   * Initializes enabled modules as reported by `getModulesIdNames` excluding
   * those that have been disabled for the current profile and returns the
   * initialized modules.
   * @param timeout Timeout in milliseconds after which initialization of a
   *     particular module aborts.
   */
  async initializeModules(timeout: number): Promise<Module[]> {
    const modulesIdNames: ModuleIdName[] =
        (await NewTabPageProxy.getInstance().handler.getModulesIdNames()).data;
    return this.initializeModulesHavingIds(
        modulesIdNames.map(m => m.id), timeout);
  }

  /**
   * Initializes a given list of modules based on the provided module ids.
   * Serves as a convenience method for cases where the caller already knows the
   * desired list of module ids to load.
   *
   * @param moduleIds A list of module ids to be leveraged when determining the
   *     modules to be initialized.
   * @param timeout Timeout in milliseconds after which initialization of a
   *     particular module aborts.
   */
  async initializeModulesHavingIds(modulesIds: string[], timeout: number):
      Promise<Module[]> {
    // Capture updateDisabledModules -> setDisabledModules round trip in a
    // promise for convenience.
    const disabledIds = await new Promise<string[]>((resolve, _) => {
      const callbackRouter = NewTabPageProxy.getInstance().callbackRouter;
      const listenerId = callbackRouter.setDisabledModules.addListener(
          (all: boolean, ids: string[]) => {
            callbackRouter.removeListener(listenerId);
            resolve(all ? this.descriptors_.map(({id}) => id) : ids);
          });
      NewTabPageProxy.getInstance().handler.updateDisabledModules();
    });
    const descriptorsMap: Map<string, ModuleDescriptor> =
        new Map(this.descriptors_.map(d => [d.id, d]));
    const descriptors: ModuleDescriptor[] =
        modulesIds.filter(id => !disabledIds.includes(id))
            .map(id => descriptorsMap.get(id)!);

    // Modules may have an updated order, e.g. because of drag&drop or a Finch
    // param. Apply the updated order such that modules without a specified
    // order (e.g. because they were just enabled or launched) land at the
    // bottom of the list.
    const orderedIds =
        (await NewTabPageProxy.getInstance().handler.getModulesOrder())
            .moduleIds;
    if (orderedIds.length > 0) {
      descriptors.sort((a, b) => {
        const aHasOrder = orderedIds.includes(a.id);
        const bHasOrder = orderedIds.includes(b.id);
        if (aHasOrder && bHasOrder) {
          // Apply order.
          return orderedIds.indexOf(a.id) - orderedIds.indexOf(b.id);
        }
        if (!aHasOrder && bHasOrder) {
          return 1;  // Move b up.
        }
        if (aHasOrder && !bHasOrder) {
          return -1;  // Move a up.
        }
        return 0;  // Keep current order.
      });
    }
    const elements =
        await Promise.all(descriptors.map(d => d.initialize(timeout)));
    return elements.map((e, i) => ({elements: e, descriptor: descriptors[i]}))
        .filter(m => !!m.elements)
        .map(m => ({
                    elements: Array.isArray(m.elements) ? m.elements :
                                                          [m.elements],
                    descriptor: m.descriptor,
                  }) as Module)
        .filter(m => m.elements.length !== 0);
  }
}
